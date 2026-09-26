# Blender: keyframes the mini succubus's flying clips on her rig (she only flies, so Mixamo's ground clips don't fit).
#   blender -b --factory-startup -P mini_succubus_flight.py -- <rigged MiniSuccubus.fbx> <output folder>
# Writes Hover, Fly, Attack, Hit and Death as armature-only FBX files, like Tools/Animations/prepare_clips.py.
#
# Each clip is a few key poses blended smoothly, plus the wingbeat on top. A pose turns bones about the world axes
# of her rig (she faces -Y, her left is +X, Z is up), parents first, in the order Y, X, Z:
#   X +: a leg or hanging arm swings back / a torso leans forward;  X -: a leg or hanging arm swings forward
#   Y +: her left arm or left wing lowers (mirrored for the right side)
# Poses are written for her left side and mirrored to the right unless the right side is given.
import bpy, math, os, sys
from mathutils import Matrix

RIG_FBX, OUT = sys.argv[sys.argv.index("--") + 1:][:2]
FPS = 30

# --- Poses ----------------------------------------------------------------------------------------------------------
HOVER = {
    "Hips": (0, 0, 0), "Spine01": (8, 0, 0), "Head": (-8, 0, 0),
    "LeftArm": (0, 55, 0), "LeftForeArm": (-35, 0, 0), "RightArm": (0, -55, 0), "RightForeArm": (-30, 0, 0),
    "LeftUpLeg": (-15, 0, 0), "LeftLeg": (35, 0, 0), "LeftFoot": (35, 0, 0),
    "RightUpLeg": (-4, 0, 0), "RightLeg": (45, 0, 0), "RightFoot": (35, 0, 0),
}
FLY = dict(HOVER, **{
    "Hips": (28, 0, 0), "Head": (-22, 0, 0),
    "LeftArm": (25, 70, 0), "RightArm": (25, -70, 0), "LeftForeArm": (-15, 0, 0), "RightForeArm": (-15, 0, 0),
    "LeftUpLeg": (12, 0, 0), "LeftLeg": (25, 0, 0), "RightUpLeg": (18, 0, 0), "RightLeg": (30, 0, 0),
})
WINDUP = dict(HOVER, **{
    "Hips": (-12, 0, 0), "Head": (-4, 0, 0),
    "RightArm": (35, 25, 0), "RightForeArm": (-45, 0, 0),   # claw raised high behind her
    "LeftArm": (-10, 45, 0),
})
STRIKE = dict(HOVER, **{
    "Hips": (26, 0, 0), "Head": (-18, 0, 0), "Spine01": (12, 0, -15),
    "RightArm": (-65, -75, 25), "RightForeArm": (-5, 0, 0),  # swept down and across in front of her
    "LeftArm": (20, 60, 0),
})
FLINCH = dict(HOVER, **{"Hips": (-22, 0, 0), "Head": (-15, 0, 0), "LeftArm": (0, 30, 0), "RightArm": (0, -30, 0)})
LIMP = {
    "Hips": (45, 0, 0), "Spine01": (15, 0, 0), "Spine": (10, 0, 0), "Head": (35, 0, 0),
    "LeftArm": (0, 80, 0), "RightArm": (0, -80, 0), "LeftForeArm": (-5, 0, 0), "RightForeArm": (-5, 0, 0),
    "LeftUpLeg": (-2, 0, 0), "LeftLeg": (12, 0, 0), "LeftFoot": (40, 0, 0),
    "RightUpLeg": (4, 0, 0), "RightLeg": (18, 0, 0), "RightFoot": (40, 0, 0),
    "LeftWing1": (0, 50, 0), "LeftWing2": (0, 30, 0), "RightWing1": (0, -50, 0), "RightWing2": (0, -30, 0),
}

# Clips: seconds, key poses as (time, pose), wingbeats per second and their size in degrees (a function of time
# so a clip can slow or stop its wings).
CLIPS = {
    "Hover": (1.0, [(0, HOVER), (1, HOVER)], 2.0, lambda t: 30),
    "Fly": (2 / 3, [(0, FLY), (1, FLY)], 3.0, lambda t: 38),
    # The claw lands at 60%: the game's enemy attacks connect at 60% of their clip.
    "Attack": (0.9, [(0, HOVER), (.42, WINDUP), (.6, STRIKE), (.72, STRIKE), (1, HOVER)], 2.2, lambda t: 30 + 12 * math.sin(math.pi * min(t / .6, 1))),
    "Hit": (0.4, [(0, HOVER), (.25, FLINCH), (1, HOVER)], 2.5, lambda t: 18),
    "Death": (1.0, [(0, HOVER), (.7, LIMP), (1, LIMP)], 2.0, lambda t: 30 * max(0, 1 - t / .5)),
}


def mirrored(pose):
    full = dict(pose)
    for name, (x, y, z) in pose.items():
        if name.startswith("Left"):
            full.setdefault("Right" + name[4:], (x, -y, -z))
    return full


def smooth(t):
    return t * t * (3 - 2 * t)


def blend(keys, u):
    """The pose at fraction u of the clip, smoothly between its key poses."""
    for (t0, a), (t1, b) in zip(keys, keys[1:]):
        if t0 <= u <= t1:
            k = smooth((u - t0) / max(t1 - t0, 1e-6))
            a, b = mirrored(a), mirrored(b)
            return {n: tuple(a.get(n, (0, 0, 0))[i] * (1 - k) + b.get(n, (0, 0, 0))[i] * k for i in range(3)) for n in set(a) | set(b)}
    return mirrored(keys[-1][1])


# --- Rig ------------------------------------------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=RIG_FBX)
rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
for o in list(bpy.data.objects):
    if o is not rig:
        bpy.data.objects.remove(o, do_unlink=True)
scene = bpy.context.scene
scene.render.fps = FPS


def depth(bone):
    return 0 if bone.parent is None else 1 + depth(bone.parent)


ORDER = sorted(rig.data.bones, key=depth)
AXES = {"X": 0, "Y": 1, "Z": 2}


def apply(pose):
    for pb in rig.pose.bones:
        pb.rotation_mode = "QUATERNION"
        pb.matrix_basis.identity()
    bpy.context.view_layer.update()
    for bone in ORDER:
        if bone.name not in pose:
            continue
        pb = rig.pose.bones[bone.name]
        x, y, z = pose[bone.name]
        for axis, degrees in (("Y", y), ("X", x), ("Z", z)):
            if abs(degrees) < 1e-3:
                continue
            world = rig.matrix_world @ pb.matrix
            head = world.translation.copy()
            turned = Matrix.Translation(head) @ Matrix.Rotation(math.radians(degrees), 4, axis) @ Matrix.Translation(-head) @ world
            pb.matrix = rig.matrix_world.inverted() @ turned
            bpy.context.view_layer.update()


os.makedirs(OUT, exist_ok=True)
for name, (seconds, keys, beats, size) in CLIPS.items():
    frames = max(2, round(seconds * FPS))
    rig.animation_data_clear()
    rig.animation_data_create()
    rig.animation_data.action = bpy.data.actions.new(name)
    looping = name in ("Hover", "Fly")
    for i in range(frames + (0 if looping else 1)):
        u = i / frames
        t = u * seconds
        pose = blend(keys, u)
        # The wingbeat: the root bone leads, the outer bones follow a little later and bend less.
        for side, sign in (("Left", 1), ("Right", -1)):
            for bone, lag, share in (("Wing1", 0, 1), ("Wing2", .05, .5), ("Wing3", .1, .3)):
                a = size(t) * share * math.sin(2 * math.pi * beats * (t - lag))
                x, y, z = pose.get(side + bone, (0, 0, 0))
                pose[side + bone] = (x, y + sign * a, z)
        apply(pose)
        for pb in rig.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=i + 1)
    scene.frame_start, scene.frame_end = 1, frames + (0 if looping else 1)
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, name + ".fbx"), use_selection=True, object_types={"ARMATURE"}, add_leaf_bones=False,
                             axis_forward="-Y", axis_up="Z", bake_anim=True, bake_anim_use_all_actions=False,
                             bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0)
    print(f"FLIGHT CLIP {name}: {scene.frame_end} frames")
    bpy.data.actions.remove(rig.animation_data.action)
print("FLIGHT CLIPS DONE")
