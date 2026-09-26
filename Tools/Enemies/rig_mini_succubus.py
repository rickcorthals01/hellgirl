# Blender: rigs the Meshy mini succubus (an unrigged T-pose mesh) with Hellgirl's 24-bone skeleton plus three wing
# bones per side, so the Mixamo clips of Tools/Animations fit onto her unchanged.
# Run through rig_mini_succubus.ps1, or:
#   blender -b --factory-startup -P rig_mini_succubus.py -- <game root> <output fbx> [preview folder]
#
# How:
#   * The skeleton is Hellgirl's (Rags) with every joint moved onto the mini succubus: joint heights and arm
#     positions are measured from her mesh (arms, legs, head), depth is the middle of her body at that height.
#   * Her legs are modelled together, so leg weights are split by side: left of the centre line only follows the
#     left leg, right only the right.
#   * Her wings sit behind her arms. They are cut out by position and weighted to their own bones (root, top and
#     outer/lower edge), which Mixamo clips don't drive: the wings keep their pose on her upper back.
#   * Everything else is weighted with Blender's automatic (heat) weights.
import bpy, bmesh, os, sys, math
from mathutils import Vector, Matrix

args = sys.argv[sys.argv.index("--") + 1:]
GAME, OUT = args[0], args[1]
PREVIEW = args[2] if len(args) > 2 else None
MINI = os.path.join(GAME, r"Meshy Models\Enemies\Mini Succubus Model\mini_succubus_enemy_m\Meshy_AI_mini_succubus_enemy_m_0925130025_image-to-3d-texture_fbx\Meshy_AI_mini_succubus_enemy_m_0925130025_image-to-3d-texture.fbx")
REF = os.path.join(GAME, r"Animation Testing\FreshHellgirl\Rags\Rags.fbx")
WING_BONES = ["LeftWing1", "LeftWing2", "LeftWing3", "RightWing1", "RightWing2", "RightWing3"]

bpy.ops.wm.read_factory_settings(use_empty=True)

# --- The mesh, standing on the floor at the origin ------------------------------------------------------------
bpy.ops.import_scene.fbx(filepath=MINI)
body = next(o for o in bpy.data.objects if o.type == "MESH")
body.name = body.data.name = "MiniSuccubus"
bpy.context.view_layer.objects.active = body
body.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
floor = min(v.co.z for v in body.data.vertices)
for v in body.data.vertices:
    v.co.z -= floor

# Her legs are modelled together, so a stride stretched the skin between them into a web. Below the crotch the
# mesh is cut along the centre line and each leg's cut side is closed, so the legs part cleanly.
CROTCH = 0.64
bm = bmesh.new()
bm.from_mesh(body.data)
legs = [f for f in bm.faces if all(v.co.z < CROTCH for v in f.verts) and any(abs(v.co.x) < 0.2 for v in f.verts)]
geom = list({v for f in legs for v in f.verts}) + list({e for f in legs for e in f.edges}) + legs
cut = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, 0), plane_no=(1, 0, 0), dist=1e-5)
seam = [e for e in cut["geom_cut"] if isinstance(e, bmesh.types.BMEdge) and all(abs(v.co.x) < 1e-4 for v in e.verts)]
bmesh.ops.split_edges(bm, edges=seam)
opened = [e for e in bm.edges if e.is_boundary and all(abs(v.co.x) < 1e-4 and v.co.z < CROTCH + 0.01 for v in e.verts)]
# Each copy of a cut vertex sits exactly on the centre line: nudge it toward the leg its neighbours belong to,
# so the side-by-side leg weights below give it to the right leg.
for v in {v for e in opened for v in e.verts}:
    side = sum(e.other_vert(v).co.x for e in v.link_edges if abs(e.other_vert(v).co.x) > 1e-4)
    v.co.x = 1e-4 if side > 0 else -1e-4
filled = bmesh.ops.holes_fill(bm, edges=opened, sides=0)["faces"]
bmesh.ops.triangulate(bm, faces=filled)
bm.to_mesh(body.data)
bm.free()
print(f"LEGS parted: {len(seam)} seam edges cut, {len(filled)} faces closing the cut sides")
V = [v.co.copy() for v in body.data.vertices]
print(f"MINI {len(V)} verts, height {max(p.z for p in V):.3f}")

# --- Measurements -----------------------------------------------------------------------------------------------
FRONT = 0.03  # the wings are all behind this depth; body and arms in front of it


def box(xlo, xhi, zlo, zhi, front_only=True):
    return [p for p in V if xlo <= p.x <= xhi and zlo <= p.z <= zhi and (p.y < FRONT or not front_only)]


def centre_y(pts, bias=0.5):
    """Depth of a joint: from the front (lowest y) toward the back by `bias` of the section's depth."""
    lo, hi = min(p.y for p in pts), max(p.y for p in pts)
    return lo + (hi - lo) * bias


def column(z, half=0.02, xlo=-0.12, xhi=0.12, bias=0.55):
    pts = box(xlo, xhi, z - half, z + half)
    return centre_y(pts, bias)


ARM_LINE = 1.165  # height where her silhouette is widest: the T-pose arms


def arm_section(x, near_z=ARM_LINE, near_y=None):
    """The arm's cross-section at distance x from the centre line (left side): of the separate runs of points
    across the arm's height band, the one in front (the wing membrane's runs lie behind, at y ~ +0.02) closest to
    the arm line. Returns the section's centre, or None past the fingertips."""
    pts = sorted(box(x - 0.012, x + 0.012, 1.0, 1.3), key=lambda p: p.z)
    runs, run = [], []
    for p in pts:
        if run and p.z - run[-1].z > 0.015:
            runs.append(run); run = []
        run.append(p)
    if run:
        runs.append(run)
    best = None
    for r in runs:
        lo, hi = r[0].z, r[-1].z
        c = Vector((x, sum(p.y for p in r) / len(r), (lo + hi) / 2))
        if c.y > -0.02:
            continue  # wing membrane, behind the arm
        if near_y is not None and abs(c.y - near_y) > 0.05:
            continue
        if abs(c.z - near_z) < 0.05 and (best is None or abs(c.z - near_z) < abs(best.z - near_z)):
            best = c
    return best


# Follow the arm outward from the shoulder until it ends: that is the fingertip.
line, x, misses = [], 0.20, 0
prev = arm_section(x)
while prev is not None and x < 0.95 and misses < 5:
    x += 0.01
    found = arm_section(x, prev.z, prev.y)
    if found is None:
        misses += 1  # a sparse slice of the mesh; the arm ends only after several empty steps
        continue
    misses = 0
    line.append(prev)
    prev = found
line.append(prev)
hand_tip = line[-1].x


def arm_point(x):
    return min(line, key=lambda p: abs(p.x - x)).copy()


def arm_radius(x):
    """How far above and below the arm line its surface reaches: a thick shoulder, a slim wrist."""
    t = max(0.0, min(1.0, (x - SHOULDER_X) / max(hand_tip - SHOULDER_X, 1e-3)))
    return 0.075 - 0.03 * t


SHOULDER_X = 0.14
# Hellgirl's arm: shoulder joint 0.143, elbow 0.341, wrist 0.578, fingertips 0.814 from the centre line.
elbow_x = SHOULDER_X + (0.341 - 0.143) / (0.814 - 0.143) * (hand_tip - SHOULDER_X)
wrist_x = SHOULDER_X + (0.578 - 0.143) / (0.814 - 0.143) * (hand_tip - SHOULDER_X)
print(f"ARM hand tip {hand_tip:.3f}, elbow {elbow_x:.3f}, wrist {wrist_x:.3f}; line " + " ".join(f"({p.x:.2f},{p.y:.3f},{p.z:.3f})" for p in line[::5]))
elbow, wrist, tip = (arm_point(x) for x in (elbow_x, wrist_x, hand_tip - 0.015))
shoulder = arm_point(0.20)
shoulder.x = SHOULDER_X

# Heights (from the floor), measured on the slices of her body.
ANKLE, KNEE, HIP_JOINT, PELVIS = 0.10, 0.43, 0.745, 0.82
SPINE02, SPINE01, SPINE, SPINE_END = 0.93, 1.04, 1.12, 1.17
NECK, HEAD, HEAD_TOP = 1.20, 1.265, 1.53
LEG_X = {HIP_JOINT: 0.065, KNEE: 0.045, ANKLE: 0.035}
toes = box(0.0, 0.1, 0.0, 0.05)
toe_tip_y = min(p.y for p in toes)
head_pts = box(-0.12, 0.12, HEAD, HEAD_TOP)
head_front = min(p.y for p in head_pts)


def leg(z, side):
    pts = box(0.0, 0.14, z - 0.02, z + 0.02) if side > 0 else box(-0.14, 0.0, z - 0.02, z + 0.02)
    return Vector((side * LEG_X[z], centre_y(pts), z))


def mid(z, bias=0.6):
    return Vector((0.0, column(z, bias=bias), z))


J = {}
J["Hips"] = (mid(PELVIS), mid(PELVIS + 0.07))
for side, name in ((1, "Left"), (-1, "Right")):
    hip, knee, ankle = leg(HIP_JOINT, side), leg(KNEE, side), leg(ANKLE, side)
    ball = Vector((side * 0.035, toe_tip_y + 0.06, 0.02))
    J[name + "UpLeg"] = (hip, knee)
    J[name + "Leg"] = (knee, ankle)
    J[name + "Foot"] = (ankle, ball)
    J[name + "ToeBase"] = (ball, Vector((side * 0.035, toe_tip_y, 0.02)))
    s = Vector((side, 1, 1))

    def mirror(p):
        return Vector((p.x * side, p.y, p.z))
    J[name + "Shoulder"] = (Vector((side * 0.03, shoulder.y, shoulder.z + 0.01)), mirror(shoulder))
    J[name + "Arm"] = (mirror(shoulder), mirror(elbow))
    J[name + "ForeArm"] = (mirror(elbow), mirror(wrist))
    J[name + "Hand"] = (mirror(wrist), mirror(tip))
J["Spine02"] = (mid(SPINE02), mid(SPINE01))
J["Spine01"] = (mid(SPINE01), mid(SPINE))
J["Spine"] = (mid(SPINE), mid(SPINE_END))
J["neck"] = (mid(NECK, 0.5), mid(HEAD, 0.5))
head_base = mid(HEAD, 0.5)
J["Head"] = (head_base, head_base + Vector((0, -0.10, 0.10)))
J["head_end"] = (Vector((0, head_base.y - 0.04, 1.45)), Vector((0, head_base.y - 0.08, HEAD_TOP + 0.1)))
J["headfront"] = (Vector((0, head_front + 0.1, HEAD)), Vector((0, head_front - 0.05, HEAD)))

# Wings: the wing mesh, found by position (behind the body plane, outside the torso, not the head, hair or horns).
TORSO_X = 0.15


def is_arm(p):
    x = abs(p.x)
    if x < TORSO_X or x > hand_tip + 0.02:
        return False
    c = arm_point(x)
    # The arm lies wholly in front of the wing membrane (y ~ +0.02): within its height band and depth of the line.
    return p.y < -0.02 and abs(p.z - c.z) <= arm_radius(x) and abs(p.y - c.y) <= 0.07


def is_wing(p):
    if abs(p.x) < 0.22 and p.z > 1.22:
        return False  # head, hair and horns
    if abs(p.x) <= TORSO_X:
        return p.y > 0.01 and p.z > 0.95  # behind her upper back: the wing roots
    return not is_arm(p)


wing = [is_wing(p) for p in V]
print(f"WING vertices: {sum(wing)} of {len(V)}")
for side, name in ((1, "Left"), (-1, "Right")):
    pts = [p for p, w in zip(V, wing) if w and p.x * side > TORSO_X]
    top = max(pts, key=lambda p: p.z)
    outer = max(pts, key=lambda p: p.x * side)
    lower = min((p for p in pts if abs(p.x) > 0.3), key=lambda p: p.z)
    root = Vector((side * 0.09, 0.07, 1.10))
    print(f"WING {name}: top {tuple(round(c, 3) for c in top)} outer {tuple(round(c, 3) for c in outer)} lower {tuple(round(c, 3) for c in lower)}")
    J[name + "Wing1"] = (root, top)
    J[name + "Wing2"] = (top, outer)
    J[name + "Wing3"] = (outer, lower)

# --- Skeleton: Hellgirl's, joints moved onto the mini succubus ----------------------------------------------
before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=REF)
new = [o for o in bpy.data.objects if o not in before]
rig = next(o for o in new if o.type == "ARMATURE")
for o in new:
    if o is not rig:
        bpy.data.objects.remove(o, do_unlink=True)
rig.name = rig.data.name = "MiniSuccubusRig"
inv = rig.matrix_world.inverted()
bpy.ops.object.select_all(action="DESELECT")
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode="EDIT")
eb = rig.data.edit_bones
for name in WING_BONES:
    b = eb.new(name)
    b.parent = eb["Spine"]
for name, (h, t) in J.items():
    b = eb[name]
    b.use_connect = False
    b.head, b.tail = inv @ h, inv @ t
missing = [b.name for b in eb if b.name not in J]
assert not missing, f"bones without a position: {missing}"
eb["LeftWing2"].parent = eb["LeftWing1"]; eb["LeftWing3"].parent = eb["LeftWing2"]
eb["RightWing2"].parent = eb["RightWing1"]; eb["RightWing3"].parent = eb["RightWing2"]
for name in WING_BONES:
    eb[name].roll = 0
bpy.ops.object.mode_set(mode="OBJECT")
print(f"RIG {len(rig.data.bones)} bones")

# --- Weights ------------------------------------------------------------------------------------------------------
# Heat weights for the body only: wing bones don't deform during this step.
for name in WING_BONES:
    rig.data.bones[name].use_deform = False
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True); rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.parent_set(type="ARMATURE_AUTO")
for name in WING_BONES:
    rig.data.bones[name].use_deform = True
groups = {g.name: g for g in body.vertex_groups}
for name in WING_BONES:
    if name not in groups:
        groups[name] = body.vertex_groups.new(name=name)
names = {g.index: g.name for g in body.vertex_groups}


def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
    return (a + ab * t - p).length


unweighted = 0
for v, p, w in zip(body.data.vertices, V, wing):
    current = {names[g.group]: g.weight for g in v.groups if g.weight > 0}
    if w:
        side = "Left" if p.x > 0 else "Right"
        new_w = {}
        for n in (side + "Wing1", side + "Wing2", side + "Wing3"):
            h, t = J[n]
            new_w[n] = 1.0 / (seg_dist(p, h, t) + 0.02) ** 3
        root = J[side + "Wing1"][0]
        d = (p - root).length
        if d < 0.12:  # blend into the back where the wing grows out of it
            new_w["Spine"] = sum(new_w.values()) * (1 - d / 0.12) * 2
    else:
        new_w = {n: x for n, x in current.items() if n not in WING_BONES}
        if p.z < 0.72:  # modelled-together legs: each side follows only its own leg
            wrong = "Right" if p.x > 0 else "Left"
            new_w = {n: x for n, x in new_w.items() if not (n.startswith(wrong) and ("Leg" in n or "Foot" in n or "Toe" in n))}
        if not new_w:  # heat weighting left it out: nearest body bone
            unweighted += 1
            best = min((n for n in J if n not in WING_BONES and n not in ("head_end", "headfront")),
                       key=lambda n: seg_dist(p, *J[n]))
            new_w = {best: 1.0}
    total = sum(new_w.values())
    for g in list(v.groups):
        body.vertex_groups[g.group].remove([v.index])
    for n, x in new_w.items():
        if x / total > 0.01:
            groups[n].add([v.index], x / total, "REPLACE")
print(f"WEIGHTS done; {unweighted} vertices had no heat weight and took their nearest bone")

# --- Checks and previews ----------------------------------------------------------------------------------------
if PREVIEW:
    os.makedirs(PREVIEW, exist_ok=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "VERTEX"
    scene.render.resolution_x, scene.render.resolution_y = 800, 800
    # Colour each vertex by its strongest bone so the regions can be checked.
    palette = {"Hips": (.9, .9, .2), "Spine02": (.9, .6, .2), "Spine01": (.9, .4, .2), "Spine": (.9, .2, .2), "neck": (.6, .2, .6),
               "Head": (.9, .5, .9), "LeftShoulder": (.2, .5, .9), "LeftArm": (.2, .7, .9), "LeftForeArm": (.2, .9, .9), "LeftHand": (.2, .9, .6),
               "RightShoulder": (.3, .3, .8), "RightArm": (.4, .4, .9), "RightForeArm": (.5, .5, 1), "RightHand": (.6, .6, 1),
               "LeftUpLeg": (.2, .8, .2), "LeftLeg": (.4, .9, .4), "LeftFoot": (.6, 1, .6), "LeftToeBase": (.8, 1, .8),
               "RightUpLeg": (.1, .5, .1), "RightLeg": (.2, .6, .2), "RightFoot": (.3, .7, .3), "RightToeBase": (.4, .8, .4),
               "LeftWing1": (.5, .1, .1), "LeftWing2": (.7, .2, .1), "LeftWing3": (.9, .4, .1),
               "RightWing1": (.1, .1, .5), "RightWing2": (.1, .2, .7), "RightWing3": (.1, .4, .9)}
    col = body.data.color_attributes.new("Bones", "FLOAT_COLOR", "POINT")
    idx = {g.index: g.name for g in body.vertex_groups}
    for v in body.data.vertices:
        best = max(v.groups, key=lambda g: g.weight, default=None)
        c = palette.get(idx[best.group], (1, 1, 1)) if best else (1, 1, 1)
        col.data[v.index].color = (*c, 1)
    body.data.color_attributes.active_color = col
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); scene.collection.objects.link(cam); scene.camera = cam
    cam.data.type = "ORTHO"; cam.data.ortho_scale = 2.1

    def shoot(file, direction, target=Vector((0, 0, 0.83))):
        cam.location = target + direction * 6
        cam.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(PREVIEW, file)
        bpy.ops.render.render(write_still=True)

    shoot("weights_front.png", Vector((0, -1, 0)))
    shoot("weights_back.png", Vector((0, 1, 0)))
    shoot("weights_side.png", Vector((1, 0, 0)))
    # A test pose: arms down, a stride, a lean and a head turn. The wings must stay on her back.
    for pb in rig.pose.bones:
        pb.rotation_mode = "XYZ"

    def turn(bone, axis, degrees):
        pb = rig.pose.bones[bone]
        # rotate about a world axis, whatever the bone's own orientation
        world = rig.matrix_world @ pb.matrix
        rot = Matrix.Rotation(math.radians(degrees), 4, axis)
        loc = world.translation.copy()
        new = Matrix.Translation(loc) @ rot @ Matrix.Translation(-loc) @ world
        pb.matrix = rig.matrix_world.inverted() @ new
        bpy.context.view_layer.update()

    turn("LeftArm", "Y", 65); turn("RightArm", "Y", -65)
    turn("LeftForeArm", "X", 40); turn("RightForeArm", "X", 40)
    turn("LeftUpLeg", "X", 30); turn("LeftLeg", "X", -35)
    turn("RightUpLeg", "X", -20)
    turn("Spine01", "X", 10); turn("Head", "Z", 25)
    shoot("pose_front.png", Vector((0, -1, 0)))
    shoot("pose_side.png", Vector((1, 0, 0)))
    shoot("pose_three_quarter.png", Vector((0.7, -0.7, 0.15)).normalized())
    for pb in rig.pose.bones:
        pb.matrix_basis.identity()
    body.data.color_attributes.remove(col)

# --- Export ---------------------------------------------------------------------------------------------------------
# Same axes as the clips of Tools/Animations (-Y forward, Z up), so the clips fit this skeleton directly.
os.makedirs(os.path.dirname(OUT), exist_ok=True)
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True); rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                         axis_forward="-Y", axis_up="Z", bake_anim=False, mesh_smooth_type="FACE", path_mode="STRIP")
print(f"RIG MINI SUCCUBUS DONE: {OUT}")
