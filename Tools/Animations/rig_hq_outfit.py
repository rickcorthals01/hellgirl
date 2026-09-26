# Blender: rigs an unrigged high-quality Meshy model of an outfit with that outfit's existing skeleton, so every
# animation clip keeps working. The old rigged model (the open-handed original in <outfit>/openhands/) gives the
# skeleton and the skin weights:
#   1. the new model is reduced to about TargetTris triangles (the textures keep the detail),
#   2. scaled to the old model's height, turned to face the same way and stood on the same spot,
#   3. given the old model's skin weights (each vertex takes the weights of the nearest point on the old surface),
#      cleaned (at most 4 bones per vertex, normalised) and bound to the old skeleton,
#   4. exported as the new open-handed original. The old files are kept in <outfit>/lowpoly/.
# fist_hands.ps1 then bakes the fists into it, re-imports it into Unreal and rebuilds the clips.
#   blender -b --factory-startup -P rig_hq_outfit.py -- <game root> <Outfit> <hq.fbx> <target tris> [preview folder]
# With a preview folder it renders an overlay of old and new and a posed test there, and writes nothing else.
import bpy, json, math, os, shutil, sys
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
game_root, outfit, hq_path, target_tris = args[0], args[1], args[2], int(args[3])
preview_dir = args[4] if len(args) > 4 else None
clips = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "clips.json")))
source = os.path.join(game_root, clips["outfits"][outfit])
folder = os.path.dirname(source)
original = os.path.join(folder, "openhands", outfit + ".fbx")


def bounds(obj):
    pts = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def fit(new, old):
    """Scales new to old's height and stands it on the same spot (feet on the same floor, same centre)."""
    olo, ohi = bounds(old)
    nlo, nhi = bounds(new)
    new.data.transform(Matrix.Scale((ohi.z - olo.z) / (nhi.z - nlo.z), 4))
    nlo, nhi = bounds(new)
    new.data.transform(Matrix.Translation(Vector(((olo.x + ohi.x) / 2 - (nlo.x + nhi.x) / 2, (olo.y + ohi.y) / 2 - (nlo.y + nhi.y) / 2, olo.z - nlo.z))))


def fit_error(new, tree, samples=4000):
    """Mean distance from a sample of the new model's vertices to the old model's surface."""
    verts = new.data.vertices
    step = max(1, len(verts) // samples)
    total = count = 0
    for i in range(0, len(verts), step):
        hit = tree.find_nearest(new.matrix_world @ verts[i].co)
        if hit[0] is not None:
            total += hit[3]
            count += 1
    return total / max(1, count)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=original)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
old = next(o for o in bpy.data.objects if o.type == "MESH")
arm.data.pose_position = "REST"
before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=hq_path)
new = next(o for o in bpy.data.objects if o not in before and o.type == "MESH")
bpy.ops.object.select_all(action="DESELECT")
new.select_set(True)
bpy.context.view_layer.objects.active = new
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
tris = sum(len(p.vertices) - 2 for p in new.data.polygons)
print(f"HQ {outfit}: {len(new.data.vertices)} verts, {tris} tris")

# 1. Reduce.
if tris > target_tris:
    dec = new.modifiers.new("Reduce", "DECIMATE")
    dec.ratio = target_tris / tris
    dec.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=dec.name)
    print(f"HQ {outfit}: reduced to {sum(len(p.vertices) - 2 for p in new.data.polygons)} tris")

# 2. Fit onto the old model: same height, feet on the same floor, centred on the same spot, and facing whichever way
# lies closer to the old surface (tried both ways round).
from mathutils.bvhtree import BVHTree
import bmesh
bm = bmesh.new()
bm.from_mesh(old.data)
bm.transform(old.matrix_world)
tree = BVHTree.FromBMesh(bm)
fit(new, old)
as_is = fit_error(new, tree)
new.data.transform(Matrix.Rotation(math.pi, 4, "Z"))
fit(new, old)
turned = fit_error(new, tree)
if as_is <= turned:
    new.data.transform(Matrix.Rotation(math.pi, 4, "Z"))
    fit(new, old)
olo, ohi = bounds(old)
nlo, nhi = bounds(new)
print(f"HQ {outfit}: fit error {min(as_is, turned) * 100:.1f} cm ({'turned round' if turned < as_is else 'as it came'}; the other way {max(as_is, turned) * 100:.1f} cm); old span {tuple(round(v, 3) for v in (ohi - olo))}, new span {tuple(round(v, 3) for v in (nhi - nlo))}")

if preview_dir:
    os.makedirs(preview_dir, exist_ok=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "OBJECT"
    scene.render.resolution_x, scene.render.resolution_y = 1400, 1100
    old.color = (1, .25, .2, 1)
    new.color = (.75, .8, .9, 1)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 2.1
    for view, d in (("front", Vector((0, -1, 0))), ("side", Vector((1, 0, 0)))):
        cam.location = Vector((0, 0, .85)) + d * 5
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        new.hide_render = False
        old.hide_render = False
        scene.display.shading.show_xray = True
        scene.display.shading.xray_alpha = .5
        scene.render.filepath = os.path.join(preview_dir, f"{outfit}_overlay_{view}.png")
        bpy.ops.render.render(write_still=True)

# 3. Skin weights from the old surface, then clean them up and bind to the old skeleton.
bpy.ops.object.select_all(action="DESELECT")
new.select_set(True)
bpy.context.view_layer.objects.active = new
transfer = new.modifiers.new("Weights", "DATA_TRANSFER")
transfer.object = old
transfer.use_vert_data = True
transfer.data_types_verts = {"VGROUP_WEIGHTS"}
transfer.vert_mapping = "POLYINTERP_NEAREST"
transfer.layers_vgroup_select_src = "ALL"
transfer.layers_vgroup_select_dst = "NAME"
bpy.ops.object.datalayout_transfer(modifier=transfer.name)
bpy.ops.object.modifier_apply(modifier=transfer.name)
bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)
unweighted = sum(1 for v in new.data.vertices if not any(g.weight > 0 for g in v.groups))
print(f"HQ {outfit}: {len(new.vertex_groups)} bone groups, {unweighted} unweighted vertices")
new.parent = arm
new.matrix_parent_inverse = arm.matrix_world.inverted()
mod = new.modifiers.new("Armature", "ARMATURE")
mod.object = arm
old_name = old.name
bpy.data.objects.remove(old, do_unlink=True)
new.name = old_name
new.data.name = old_name
arm.data.pose_position = "POSE"

if preview_dir:
    # A posed test: arms raised forward, head turned, one knee up.
    for bone, axis, angle in (("LeftArm", "X", -70), ("RightArm", "X", -40), ("Head", "Z", 30), ("LeftUpLeg", "X", -60), ("LeftLeg", "X", 70), ("Spine01", "Y", 15)):
        pb = arm.pose.bones.get(bone)
        if pb:
            pb.rotation_mode = "XYZ"
            setattr(pb.rotation_euler, axis.lower(), math.radians(angle))
    bpy.context.view_layer.update()
    scene = bpy.context.scene
    scene.display.shading.show_xray = False
    for view, d in (("posed_front", Vector((0, -1, 0))), ("posed_side", Vector((1, 0, 0)))):
        cam = scene.camera
        cam.location = Vector((0, 0, .85)) + d * 5
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(preview_dir, f"{outfit}_{view}.png")
        bpy.ops.render.render(write_still=True)
    print("HQ RIG PREVIEW DONE")
    sys.exit(0)

# 4. Export as the new open-handed original; keep the old files.
keep = os.path.join(folder, "lowpoly")
os.makedirs(keep, exist_ok=True)
for path, name in ((original, outfit + "_openhands.fbx"), (source, outfit + ".fbx")):
    if os.path.exists(path) and not os.path.exists(os.path.join(keep, name)):
        shutil.copy2(path, os.path.join(keep, name))
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
new.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.fbx(filepath=original, use_selection=True, object_types={"ARMATURE", "MESH"},
                         bake_anim=False, add_leaf_bones=False, axis_forward="-Y", axis_up="Z")
print(f"HQ RIG DONE: {original}")
