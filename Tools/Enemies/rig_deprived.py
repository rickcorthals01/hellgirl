# Blender: rigs the Deprived (an unrigged Meshy model standing in an A-pose, long hair all round) onto Hellgirl's own
# skeleton, so it plays her clips directly (the imported mesh shares /Game/Hellgirl/Outfits/Rags/Rags_Skeleton):
#   1. reduces the model to about TargetTris triangles and fits it onto Hellgirl's Rags body (scaled so the bodies match,
#      centred on her, facing -Y),
#   2. poses her skeleton into the Deprived's stance (each arm fitted down and forward onto the Deprived's, unless ArmDown
#      and ArmForward are given; legs apart by LegOut degrees, one leg back a step by LegBack degrees),
#   3. gives each vertex skin weights: on the skin, those of the nearest point of Hellgirl's posed body; hair and claws
#      follow the skin they grow from, the hair fading into the head, neck and spine, so it sways like a cape instead of
#      stretching with the arms,
#   4. un-poses it exactly back into her T-pose rest (inverting each vertex's blend) and binds it to her skeleton,
#      cutting the few faces where Meshy fused hair to the arms (they would tear into sheets when the arms swing),
#   5. writes out.fbx (armature and mesh, like rig_hq_outfit.py) and, with a preview folder, overlay and posed renders.
# Run through Tools/Enemies/deprived.ps1, or:
#   blender -b --factory-startup -P rig_deprived.py -- <rigged Rags.fbx> <model.fbx> <target tris> <out.fbx> <preview folder>
#       <mode: fit|rig> [ArmDown] [LegOut] [Scale] [LegBack] [BackLeg] [ArmForward]
import bpy, bmesh, heapq, math, os, sys
from mathutils import Matrix, Vector, kdtree
from mathutils.bvhtree import BVHTree

args = sys.argv[sys.argv.index("--") + 1:]
rags_path, model_path, target_tris, out_path, preview_dir, mode = args[0], args[1], int(args[2]), args[3], args[4], args[5]
ARM_DOWN = float(args[6]) if len(args) > 6 else 0.0     # 0: fit each arm on its own
LEG_OUT = float(args[7]) if len(args) > 7 else 4.0
SCALE = float(args[8]) if len(args) > 8 else 0.0      # 0: work it out from the height
LEG_BACK = float(args[9]) if len(args) > 9 else 16.0  # her right leg stands back a step
BACK_LEG = args[10] if len(args) > 10 else "RightUpLeg"
ARM_FORWARD = float(args[11]) if len(args) > 11 else 20.0  # its arms hang a little in front of the body
os.makedirs(preview_dir, exist_ok=True)
HAIR_BONES = ("Head", "neck", "Spine", "Spine01", "Spine02", "Hips")
NEAR = .05        # the Deprived's skin lies within NEAR of Hellgirl's posed body
HAIR_FADE = .10   # hair follows the skin it grows from, fading into the spine over this far along the mesh
ARM_IN, ARM_OUT = .045, .06   # only the arm itself (this close to its bones) moves with the upper arm and forearm
ARM_HAIR_FADE = .025
TEAR = 2.5              # an edge joining arm and hair stretched this many times its length is cut

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rags_path)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
body = next(o for o in bpy.data.objects if o.type == "MESH")
before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=model_path)
dep = next(o for o in bpy.data.objects if o not in before and o.type == "MESH")
bpy.ops.object.select_all(action="DESELECT")
dep.select_set(True)
bpy.context.view_layer.objects.active = dep
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
tris = sum(len(p.vertices) - 2 for p in dep.data.polygons)
if tris > target_tris:
    dec = dep.modifiers.new("Reduce", "DECIMATE")
    dec.ratio = target_tris / tris
    dec.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=dec.name)
print(f"DEPRIVED: {tris} tris reduced to {sum(len(p.vertices) - 2 for p in dep.data.polygons)}")


def world_verts(obj, evaluated=False):
    if evaluated:
        ev = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        return [obj.matrix_world @ v.co for v in ev.data.vertices]
    return [obj.matrix_world @ v.co for v in obj.data.vertices]


def band(points, z0, z1):
    return [p for p in points if z0 <= p.z <= z1]


# 1. Fit: feet on her floor; scaled so the shoulders (the widest body line under the head) match hers; centred on her
# across (X) and at the chest's front (Y), since the hair hangs behind the body.
rest = world_verts(body)
dv = world_verts(dep)
floor = min(p.z for p in dv)
dep.data.transform(Matrix.Translation(Vector((0, 0, -floor))))
dv = world_verts(dep)
height = max(p.z for p in dv)
scale = SCALE or 1.70 / (height * .93)   # her hair rises a little above her head
dep.data.transform(Matrix.Scale(scale, 4))
dv = world_verts(dep)
legs_rest = band(rest, .3, .6)
legs_dep = [p for p in band(dv, .3, .6) if abs(p.x) < .3]
cx = sum(p.x for p in legs_rest) / len(legs_rest) - sum(p.x for p in legs_dep) / len(legs_dep)
# The fronts of the shins line up (strands of hair hang in front of the chest, but behind the legs).
cy = min(p.y for p in band(rest, .3, .5) if abs(p.x) < .25) - min(p.y for p in band(dv, .3, .5) if abs(p.x) < .25)
dep.data.transform(Matrix.Translation(Vector((cx, cy, 0))))
print(f"DEPRIVED: scale {scale:.3f}, moved ({cx:.3f}, {cy:.3f}); height now {max(p.z for p in world_verts(dep)):.3f}")

# 2. Her skeleton in the Deprived's stance: rotations around each bone's head, in armature space. Each arm is fitted on
# its own (the model isn't symmetric): the down and forward angles that run her forearm and hand through the most of
# the Deprived's vertices.
dv = world_verts(dep)
dtree = kdtree.KDTree(len(dv))
for i, p in enumerate(dv):
    dtree.insert(p, i)
dtree.balance()
turn = arm.matrix_world.to_3x3()


def fit_arm(side, sign):
    bones = arm.data.bones
    shoulder = arm.matrix_world @ bones[side + "Arm"].head_local
    samples = []
    for name in (side + "ForeArm", side + "Hand"):
        a, b = arm.matrix_world @ bones[name].head_local, arm.matrix_world @ bones[name].tail_local
        samples += [a.lerp(b, k / 11) for k in range(12)]
    best = (-1, 0.0, 0.0)
    for down in [20 + 2.5 * k for k in range(23)]:
        for forward in [-45 + 2.5 * k for k in range(37)]:
            r = (Matrix.Rotation(math.radians(-forward), 3, (turn @ Vector((1, 0, 0))).normalized())
                 @ Matrix.Rotation(math.radians(sign * down), 3, (turn @ Vector((0, 1, 0))).normalized()))
            hit = set()
            for s in samples:
                hit.update(i for _, i, _ in dtree.find_range(shoulder + r @ (s - shoulder), .045))
            if len(hit) > best[0]:
                best = (len(hit), down, forward)
    print(f"DEPRIVED: {side} arm down {best[1]:.1f}, forward {best[2]:.1f} ({best[0]} vertices on it)")
    return best[1], best[2]


left_down, left_forward = (ARM_DOWN, ARM_FORWARD) if ARM_DOWN > 0 else fit_arm("Left", 1)
right_down, right_forward = (ARM_DOWN, ARM_FORWARD) if ARM_DOWN > 0 else fit_arm("Right", -1)
arm.data.pose_position = "POSE"
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="POSE")
for name, axis, angle in (("LeftArm", "Y", left_down), ("RightArm", "Y", -right_down), ("LeftUpLeg", "Y", -LEG_OUT), ("RightUpLeg", "Y", LEG_OUT), (BACK_LEG, "X", LEG_BACK),
                         ("LeftArm", "X", -left_forward), ("RightArm", "X", -right_forward)):
    pb = arm.pose.bones[name]
    head = pb.head.copy()
    pb.matrix = Matrix.Translation(head) @ Matrix.Rotation(math.radians(angle), 4, axis) @ Matrix.Translation(-head) @ pb.matrix
    bpy.context.view_layer.update()
bpy.ops.object.mode_set(mode="OBJECT")
bpy.context.view_layer.update()

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "OBJECT"
scene.render.resolution_x, scene.render.resolution_y = 1100, 1100
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = "ORTHO"
cam.data.ortho_scale = 2.2


def render(tag, xray):
    scene.display.shading.show_xray = xray
    scene.display.shading.xray_alpha = .5
    for view, d in (("front", Vector((0, -1, 0))), ("side", Vector((1, 0, 0)))):
        cam.location = Vector((0, 0, .95)) + d * 6
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(preview_dir, f"{tag}_{view}.png")
        bpy.ops.render.render(write_still=True)


body.color = (1, .25, .2, 1)
dep.color = (.75, .8, .9, 1)
render("fit", True)
if mode == "fit":
    print("DEPRIVED FIT DONE")
    sys.exit(0)

# 3. Skin weights.
posed = world_verts(body, evaluated=True)
bm = bmesh.new()
ev = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
bm.from_mesh(ev.data)
bm.transform(body.matrix_world)
surface = BVHTree.FromBMesh(bm)
tree = kdtree.KDTree(len(posed))
for i, p in enumerate(posed):
    tree.insert(p, i)
tree.balance()
group_names = {g.index: g.name for g in body.vertex_groups}
body_weights = [{group_names[g.group]: g.weight for g in v.groups if g.weight > 0} for v in body.data.vertices]
segments = {}
for name in HAIR_BONES:
    pb = arm.pose.bones[name]
    segments[name] = (arm.matrix_world @ pb.head, arm.matrix_world @ pb.tail)


ARM_CHAIN = {}
for side in ("Left", "Right"):
    for name in (side + "Arm", side + "ForeArm"):
        pb = arm.pose.bones[name]
        ARM_CHAIN[name] = (arm.matrix_world @ pb.head, arm.matrix_world @ pb.tail)


def to_segment(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-8)))
    return (a + ab * t - p).length


def from_body(p):
    total, out = 0.0, {}
    for co, index, dist in tree.find_n(p, 4):
        w = 1.0 / max(dist, 1e-4)
        total += w
        for name, weight in body_weights[index].items():
            out[name] = out.get(name, 0.0) + weight * w
    # Hair draped over a shoulder lies on the arm too, but further from the bone than the Deprived's thin arm: it
    # loses the arm's weight (fading out between ARM_IN and ARM_OUT), or it would tear into a sheet when the arm lifts.
    for name, (a, b) in ARM_CHAIN.items():
        if out.get(name):
            keep = 1.0 - min(1.0, max(0.0, (to_segment(p, a, b) - ARM_IN) / (ARM_OUT - ARM_IN)))
            out[name] *= keep
    total = sum(out.values())
    if total < 1e-3:
        return from_spine(p)
    return {k: v / total for k, v in out.items() if v > 0}


def from_spine(p):
    near = []
    for name, (a, b) in segments.items():
        ab = b - a
        t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-8)))
        near.append(((a + ab * t - p).length, name))
    near.sort()
    (d0, n0), (d1, n1) = near[0], near[1]
    w0, w1 = 1.0 / max(d0, 1e-3), 1.0 / max(d1, 1e-3)
    return {n0: w0 / (w0 + w1), n1: w1 / (w0 + w1)}


# Only the Deprived's main skin (the largest connected patch lying on her body) takes body weights: hair that merely
# brushes an arm is a separate patch. Everything else (hair, claws, spikes) takes the weights of the skin it grows from,
# found by walking along the mesh, fading into the spine the further it reaches (hands and feet keep theirs: claws).
dverts = world_verts(dep)
count = len(dverts)
dist = [surface.find_nearest(p)[3] for p in dverts]
links = [[] for _ in range(count)]
for e in dep.data.edges:
    a, b = e.vertices
    length = (dverts[a] - dverts[b]).length
    links[a].append((b, length))
    links[b].append((a, length))
patch = [-1] * count
sizes = []
for i in range(count):
    if dist[i] <= NEAR and patch[i] < 0:
        patch[i] = len(sizes)
        stack, size = [i], 0
        while stack:
            v = stack.pop()
            size += 1
            for u, _ in links[v]:
                if dist[u] <= NEAR and patch[u] < 0:
                    patch[u] = patch[i]
                    stack.append(u)
        sizes.append(size)
skin_patch = max(range(len(sizes)), key=sizes.__getitem__)
skin = [patch[i] == skin_patch for i in range(count)]
print(f"DEPRIVED: {len(sizes)} patches on her body, skin {sizes[skin_patch]} vertices, largest others {sorted(sizes)[-4:-1]}")
reach = [math.inf] * count
source = [-1] * count
heap = []
for i in range(count):
    if skin[i]:
        reach[i], source[i] = 0.0, i
        heap.append((0.0, i))
heapq.heapify(heap)
while heap:
    g, v = heapq.heappop(heap)
    if g > reach[v]:
        continue
    for u, length in links[v]:
        if g + length < reach[u]:
            reach[u], source[u] = g + length, source[v]
            heapq.heappush(heap, (g + length, u))
EXTREMITY = ("Hand", "Foot", "Toe", "Thumb", "Index", "Middle", "Ring", "Pinky")
skin_weights = {}
weights = []
hair = 0
for i, p in enumerate(dverts):
    if skin[i]:
        w = skin_weights.setdefault(i, from_body(p))
    elif source[i] < 0:
        w = from_spine(p); hair += 1
    else:
        base = skin_weights.setdefault(source[i], from_body(dverts[source[i]]))
        extremity = sum(v for k, v in base.items() if any(x in k for x in EXTREMITY)) > .5
        on_arm = sum(v for k, v in base.items() if k in ARM_CHAIN) > .2   # hair fused to an arm lets go of it quickly
        t = 0.0 if extremity else min(1.0, reach[i] / (ARM_HAIR_FADE if on_arm else HAIR_FADE))
        if t >= 1.0:
            hair += 1
        spine = from_spine(p) if t > 0 else {}
        w = {k: base.get(k, 0.0) * (1 - t) + spine.get(k, 0.0) * t for k in set(base) | set(spine)}
    top = sorted(w.items(), key=lambda kv: -kv[1])[:4]
    s = sum(v for _, v in top) or 1.0
    weights.append({k: v / s for k, v in top})
print(f"DEPRIVED: {hair} of {len(weights)} vertices weighted as hair")
# Which parts count as skin (red) and which as hair (grey), over her posed body.
colours = dep.data.color_attributes.new("Skin", "FLOAT_COLOR", "POINT")
for i in range(count):
    colours.data[i].color = (.9, .2, .15, 1) if skin[i] else (.7, .75, .8, 1)
scene.display.shading.color_type = "VERTEX"
body.hide_render = True
render("skin", False)
body.hide_render = False
scene.display.shading.color_type = "OBJECT"
dep.data.color_attributes.remove(colours)
for name in {n for w in weights for n in w}:
    dep.vertex_groups.new(name=name)
for i, w in enumerate(weights):
    for name, value in w.items():
        dep.vertex_groups[name].add([i], value, "REPLACE")

# 4. Back into her T-pose rest: invert each vertex's blended bone matrix (armature pose -> rest).
deform = {}
for pb in arm.pose.bones:
    deform[pb.name] = arm.matrix_world @ pb.matrix @ pb.bone.matrix_local.inverted() @ arm.matrix_world.inverted()
for i, v in enumerate(dep.data.vertices):
    m = Matrix(((0, 0, 0, 0), (0, 0, 0, 0), (0, 0, 0, 0), (0, 0, 0, 0)))
    for name, value in weights[i].items():
        m = m + deform[name] * value
    v.co = dep.matrix_world.inverted() @ (m.inverted() @ (dep.matrix_world @ v.co))
for pb in arm.pose.bones:
    pb.matrix_basis = Matrix.Identity(4)
bpy.context.view_layer.update()

# Meshy fused some hair to the arms: faces joining the arm to hair (the arm's weight jumps across them, where a real
# armpit blends) that tear open (stretch past TEAR) when the arms swing far down, up, forward or back are cut away,
# instead of stretching into sheets.
rest_world = [dep.matrix_world @ v.co for v in dep.data.vertices]
edge_ends = [tuple(e.vertices) for e in dep.data.edges]
fit_length = [max((dverts[a] - dverts[b]).length, 1e-4) for a, b in edge_ends]
stretch = [1.0] * len(edge_ends)
for moves in ((("LeftArm", "Y", 75), ("RightArm", "Y", -75)), (("LeftArm", "Y", -60), ("RightArm", "Y", 60)),
              (("LeftArm", "X", -80), ("RightArm", "X", -80)), (("LeftArm", "X", 40), ("RightArm", "X", 40))):
    for name, axis, angle in moves:
        pb = arm.pose.bones[name]
        head = pb.head.copy()
        pb.matrix = Matrix.Translation(head) @ Matrix.Rotation(math.radians(angle), 4, axis) @ Matrix.Translation(-head) @ pb.matrix
        bpy.context.view_layer.update()
    moved = {pb.name: arm.matrix_world @ pb.matrix @ pb.bone.matrix_local.inverted() @ arm.matrix_world.inverted()
             for pb in arm.pose.bones}
    posed_at = []
    for i, co in enumerate(rest_world):
        m = Matrix(((0, 0, 0, 0), (0, 0, 0, 0), (0, 0, 0, 0), (0, 0, 0, 0)))
        for name, value in weights[i].items():
            m = m + moved[name] * value
        posed_at.append(m @ co)
    for k, (a, b) in enumerate(edge_ends):
        stretch[k] = max(stretch[k], (posed_at[a] - posed_at[b]).length / fit_length[k])
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
on_arm = [sum(v for k, v in w.items() if k in ARM_CHAIN) for w in weights]
bm = bmesh.new()
bm.from_mesh(dep.data)
bm.edges.ensure_lookup_table()
torn = [f for f in bm.faces
        if any(stretch[e.index] > TEAR for e in f.edges)
        and max(on_arm[v.index] for v in f.verts) - min(on_arm[v.index] for v in f.verts) > .6]
bmesh.ops.delete(bm, geom=torn, context="FACES_ONLY")
loose = [v for v in bm.verts if not v.link_faces]
bmesh.ops.delete(bm, geom=loose, context="VERTS")
bm.to_mesh(dep.data)
bm.free()
print(f"DEPRIVED: cut {len(torn)} faces where hair tore away from the arms")
dep.parent = arm
dep.matrix_parent_inverse = arm.matrix_world.inverted()
mod = dep.modifiers.new("Armature", "ARMATURE")
mod.object = arm
render("rest", True)

# A posed test: arms raised forward, head turned, one knee up, a lean; then back to rest.
for bone, axis, angle in (("LeftArm", "X", -70), ("RightArm", "X", -40), ("Head", "Z", 30), ("LeftUpLeg", "X", -60), ("LeftLeg", "X", 70), ("Spine01", "Y", 15)):
    pb = arm.pose.bones.get(bone)
    pb.rotation_mode = "XYZ"
    setattr(pb.rotation_euler, axis.lower(), math.radians(angle))
bpy.context.view_layer.update()
body.hide_render = True
render("posed", False)
for pb in arm.pose.bones:
    pb.matrix_basis = Matrix.Identity(4)
bpy.context.view_layer.update()

# 5. Export: her skeleton and the Deprived's mesh (Hellgirl's body is left out).
bpy.data.objects.remove(body, do_unlink=True)
dep.name = dep.data.name = "Deprived"
os.makedirs(os.path.dirname(out_path), exist_ok=True)
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
dep.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.fbx(filepath=out_path, use_selection=True, object_types={"ARMATURE", "MESH"},
                         bake_anim=False, add_leaf_bones=False, axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE")
print(f"DEPRIVED RIG DONE: {out_path}")
