# Blender: fits Mixamo clips onto every same-rig Hellgirl outfit and exports one FBX per outfit/clip.
# Run through build.ps1, or:
#   blender -b --factory-startup -P prepare_clips.py -- <clips.json> <game root> <output folder> [Clip,Clip|-] [Outfit,Outfit|-]
#
# Each bone keeps its own rest pose and receives the source bone's rotation *relative to the source
# rest pose* (world space), so small rest-orientation differences between Meshy models do not skew
# the motion. Hip motion is scaled by hip height, and net horizontal travel is removed because the
# game moves the capsule itself (dashes, lunges); the body's sway within the move is kept.
import bpy, json, os, sys
from mathutils import Matrix

args = sys.argv[sys.argv.index("--") + 1:]
config_path, game_root, out_root = args[0], args[1], args[2]
only = [c for c in (args[3].split(",") if len(args) > 3 else []) if c and c != "-"]
only_outfits = [o for o in (args[4].split(",") if len(args) > 4 else []) if o and o != "-"]
cfg = json.load(open(config_path))
mixamo = os.path.join(game_root, "Animations Mixamo")
MIRROR = Matrix.Diagonal((-1.0, 1.0, 1.0))  # The models face -Y, so world X is the character's side axis.
report = {}


def mirror_name(name):
    return name.replace("Left", "\0").replace("Right", "Left").replace("\0", "Right")


def resolve(name):
    """Returns (source file, mirrored, trim) or (None, ...) when the clip has no source yet."""
    clip = cfg["clips"][name]
    path = os.path.join(mixamo, clip["file"])
    if os.path.exists(path):
        return path, False, clip.get("trim")
    other = clip.get("mirror_of")
    if other:
        path = os.path.join(mixamo, cfg["clips"][other]["file"])
        if os.path.exists(path):
            return path, True, cfg["clips"][other].get("trim")
    return None, False, None


def rot3(matrix):
    return matrix.to_3x3().normalized()


def import_armature(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    return next(o for o in new if o.type == "ARMATURE"), new


def depth(bone):
    return 0 if bone.parent is None else 1 + depth(bone.parent)


def retarget(target, name, source_path, mirrored, trim, out_path, strike=None, contact=None):
    scene = bpy.context.scene
    source, imported = import_armature(source_path)
    action = source.animation_data.action
    start, end = trim or [int(round(v)) for v in action.frame_range]
    bones = sorted(target.data.bones, key=depth)
    root = bones[0].name
    src_names = {b.name: (mirror_name(b.name) if mirrored else b.name) for b in bones}
    missing = [n for n in src_names.values() if n not in source.data.bones]
    if missing:
        raise RuntimeError(f"{name}: source lacks bones {missing}")

    S_world, T_world = source.matrix_world, target.matrix_world
    T_world_rot_inv = rot3(T_world).inverted()
    T_world_inv = T_world.inverted()
    src_rest = {}
    for b in bones:
        r = rot3(S_world @ source.data.bones[src_names[b.name]].matrix_local)
        src_rest[b.name] = MIRROR @ r @ MIRROR if mirrored else r
    tgt_rest = {b.name: rot3(T_world @ b.matrix_local) for b in bones}
    src_root_head = (S_world @ source.data.bones[src_names[root]].matrix_local).translation
    tgt_root_head = (T_world @ target.data.bones[root].matrix_local).translation
    height_ratio = tgt_root_head.z / max(src_root_head.z, 1e-4)

    # Pass 1: sample the source.
    frames = list(range(start, end + 1))
    samples = []
    reach = []
    for f in frames:
        scene.frame_set(f)
        pose = {}
        for b in bones:
            r = rot3(S_world @ source.pose.bones[src_names[b.name]].matrix)
            pose[b.name] = MIRROR @ r @ MIRROR if mirrored else r
        hips = (S_world @ source.pose.bones[src_names[root]].matrix).translation
        offset = hips - src_root_head
        if mirrored:
            offset.x = -offset.x
        samples.append((pose, offset * height_ratio))
        if strike:
            limb = (S_world @ source.pose.bones[src_names[strike]].matrix).translation
            reach.append((limb - hips).to_2d().length)

    # Contact: an explicit source frame, else the frame where the striking limb is farthest from the hips.
    if contact is not None:
        contact_index = min(max(contact - start, 0), len(frames) - 1)
    elif reach:
        contact_index = max(range(len(reach)), key=reach.__getitem__)
    else:
        contact_index = None

    # Remove net horizontal travel (linear from first to last frame), keeping sway and all vertical motion.
    first, last = samples[0][1].copy(), samples[-1][1].copy()
    for i, (_, offset) in enumerate(samples):
        t = i / max(1, len(samples) - 1)
        offset.x -= first.x + (last.x - first.x) * t
        offset.y -= first.y + (last.y - first.y) * t

    # Pass 2: write keys on the target, parents first.
    target.animation_data_clear()
    target.animation_data_create()
    target.animation_data.action = bpy.data.actions.new(name)
    for pb in target.pose.bones:
        pb.rotation_mode = "QUATERNION"
    worst = 0.0
    for i, (pose, offset) in enumerate(samples):
        frame = i + 1
        pose_arm = {}
        for b in bones:
            rest_rel = b.parent.matrix_local.inverted() @ b.matrix_local if b.parent else b.matrix_local
            parent_pose = pose_arm[b.parent.name] if b.parent else Matrix.Identity(4)
            desired_rot = T_world_rot_inv @ (pose[b.name] @ src_rest[b.name].inverted() @ tgt_rest[b.name])
            if b.parent:
                location = (parent_pose @ rest_rel).translation
            else:
                location = b.matrix_local.translation + T_world_inv.to_3x3() @ offset
            desired = Matrix.LocRotScale(location, desired_rot, None)
            basis = rest_rel.inverted() @ parent_pose.inverted() @ desired
            pose_arm[b.name] = desired
            pb = target.pose.bones[b.name]
            pb.rotation_quaternion = basis.to_quaternion()
            pb.keyframe_insert("rotation_quaternion", frame=frame)
            if not b.parent:
                pb.location = basis.translation
                pb.keyframe_insert("location", frame=frame)

    # Fit check on the pose Blender actually evaluates from the written keys: every bone's
    # world-space change from rest must match the source's.
    for i in range(0, len(samples), max(1, len(samples) // 6)):
        scene.frame_set(i + 1)
        pose = samples[i][0]
        for b in bones:
            want = pose[b.name] @ src_rest[b.name].inverted()
            got = rot3(T_world @ target.pose.bones[b.name].matrix) @ tgt_rest[b.name].inverted()
            worst = max(worst, want.to_quaternion().rotation_difference(got.to_quaternion()).angle)

    scene.frame_start, scene.frame_end = 1, len(samples)
    bpy.ops.object.select_all(action="DESELECT")
    target.select_set(True)
    bpy.context.view_layer.objects.active = target
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=out_path, use_selection=True, object_types={"ARMATURE"}, add_leaf_bones=False,
                             axis_forward="-Y", axis_up="Z", bake_anim=True, bake_anim_use_all_actions=False,
                             bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0)
    for obj in imported:
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.actions.remove(action)
    bpy.data.actions.remove(target.animation_data.action)
    target.animation_data_clear()
    result = {"frames": len(samples), "fps": scene.render.fps, "mirrored": mirrored,
              "source": os.path.relpath(source_path, mixamo), "fit_error_deg": round(worst * 57.2958, 3)}
    if contact_index is not None:
        result["contact_frame"] = start + contact_index
        result["contact_fraction"] = round(contact_index / max(1, len(frames) - 1), 4)
    return result


for outfit, rel in cfg["outfits"].items():
    if only_outfits and outfit not in only_outfits:
        continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    target, _ = import_armature(os.path.join(game_root, rel))
    for pb in target.pose.bones:
        pb.matrix_basis.identity()
    report[outfit] = {}
    for name in cfg["clips"]:
        if only and name not in only:
            continue
        path, mirrored, trim = resolve(name)
        if not path:
            report[outfit][name] = {"skipped": "no source file yet"}
            continue
        clip = cfg["clips"][name]
        report[outfit][name] = retarget(target, name, path, mirrored, trim, os.path.join(out_root, outfit, name + ".fbx"),
                                        clip.get("strike"), clip.get("contact"))
        print(f"CLIP {outfit}/{name}: {report[outfit][name]}")
    # Re-posed outfits: re-fit their own original neutral pose onto the new rest pose.
    folder = cfg.get("outfit_clip_folders", {}).get(outfit)
    for name, file in (cfg.get("outfit_clips", {}).items() if folder else []):
        if only and name not in only:
            continue
        path = os.path.join(game_root, folder, file)
        if not os.path.exists(path):
            report[outfit][name] = {"skipped": "no source file yet"}
            continue
        report[outfit][name] = retarget(target, name, path, False, None, os.path.join(out_root, outfit, name + ".fbx"))
        print(f"CLIP {outfit}/{name}: {report[outfit][name]}")
json.dump(report, open(os.path.join(out_root, "prepare_report.json"), "w"), indent=1)
print("PREPARE CLIPS DONE")
