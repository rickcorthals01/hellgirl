# Blender: the Meshy outfits have no finger bones, so every animation shows open hands. This bakes a loose
# fist into each outfit's mesh: the four fingers curl together at three knuckles, the thumb folds over them.
# Only vertices move; the skeleton is untouched, so the game and the animation clips need no changes.
# The open-handed originals are kept in <outfit folder>/openhands/ and are always the input, so re-running is safe.
# Run through fist_hands.ps1, or:
#   blender -b --factory-startup -P fist_hands.py -- <game root> <Outfit,Outfit> [preview folder]
# With a preview folder it only renders close-ups of the new hands there and leaves the outfit files alone.
import bpy, json, math, os, shutil, sys
from mathutils import Matrix, Quaternion, Vector

args = sys.argv[sys.argv.index("--") + 1:]
game_root, outfits = args[0], args[1].split(",")
render_dir = args[2] if len(args) > 2 else None
clips = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "clips.json")))

# Loose fist, in degrees: knuckle, middle and last finger joints. The thumb turns toward the curled
# middle finger bones ("amount" of the way; "toward_thumb" shifts that spot back toward the thumb side).
CURL = (80.0, 90.0, 80.0)
THUMB = {"amount": 1.0, "toward_thumb": 0.0}


def smooth(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)


def components(indices, adj, minsize):
    seen, comps = set(), []
    for i in indices:
        if i in seen:
            continue
        stack, comp = [i], []
        seen.add(i)
        while stack:
            k = stack.pop()
            comp.append(k)
            for n in adj.get(k, ()):
                if n in indices and n not in seen:
                    seen.add(n)
                    stack.append(n)
        if len(comp) >= minsize:
            comps.append(comp)
    return comps


def fist(mesh, arm, side, adj):
    W = mesh.matrix_world
    g = mesh.vertex_groups[side + "Hand"]
    verts = mesh.data.vertices
    # Vertices that belong mainly to the hand bone.
    hand = {}
    for v in verts:
        best = max(v.groups, key=lambda x: x.weight, default=None)
        if best and best.group == g.index and best.weight > .3:
            hand[v.index] = W @ v.co
    wrist = arm.matrix_world @ arm.data.bones[side + "Hand"].head_local
    centre = sum(hand.values(), Vector()) / len(hand)
    a = (centre - wrist).normalized()                       # along the hand, toward the fingertips
    s = {i: (p - wrist).dot(a) for i, p in hand.items()}
    tip = sorted(s.values())[int(len(s) * .995)]

    # The thumb is the first part to split off when the hand is cut ever closer to the fingertips.
    minsize = max(8, len(hand) // 60)
    thumb, split = None, None
    cut = 0.0
    while cut < tip:
        comps = components({i for i in hand if s[i] > cut}, adj, minsize)
        if len(comps) >= 2:
            comps.sort(key=len)
            thumb, split = set(comps[0]), cut
            break
        cut += .003
    if thumb is None:
        raise RuntimeError(f"{side} hand: no thumb found")
    thumb_c = sum((hand[i] for i in thumb), Vector()) / len(thumb)

    # Hand frame: palm normal from the flat part of the hand, pointing to the palm side (right hands are
    # mirror images of left ones, so the thumb's side gives the palm's side).
    plate = [hand[i] for i in hand if i not in thumb and .3 * tip < s[i] < .7 * tip]
    mid = sum(plate, Vector()) / len(plate)
    cov = Matrix(((0,) * 3,) * 3)
    for p in plate:
        d = p - mid
        for r in range(3):
            for c in range(3):
                cov[r][c] += d[r] * d[c]
    normal = min((Vector(col) for col in eigenvectors(cov)), key=lambda e: (cov @ e).length)
    normal = (normal - a * normal.dot(a)).normalized()
    to_thumb = thumb_c - centre
    to_thumb -= a * to_thumb.dot(a)
    palm_guess = (a.cross(to_thumb)) * (1 if side == "Left" else -1)
    palm = normal if normal.dot(palm_guess) > 0 else -normal
    across = palm.cross(a)                                   # completes the frame
    axis = a.cross(palm)                                     # turning about this moves fingertips to the palm

    mcp = split + (tip - split) * .15                        # knuckle line
    fingers = [i for i in hand if i not in thumb]
    pos = dict(hand)                                         # working positions

    # 1. Straighten spread fingers so they curl into the palm instead of sideways. Only where the model
    #    has separate fingers; fused (gloved) fingers cannot be spread anyway.
    best, cut, rays = [], split, []
    while cut < tip - .01:
        comps = [c for c in components({i for i in fingers if s[i] > cut}, adj, minsize)]
        if len(comps) > len(best):
            best, best_cut = comps, cut
        cut += .003
    straightened = []
    if len(best) >= 3:
        rays = []
        for comp in best:
            lo = [hand[i] for i in comp if s[i] < best_cut + .01]
            hi = [hand[i] for i in comp if s[i] > best_cut + .6 * (max(s[k] for k in comp) - best_cut)]
            if not lo or not hi:
                continue
            base = sum(lo, Vector()) / len(lo)
            d = sum(hi, Vector()) / len(hi) - base
            d -= palm * d.dot(palm)
            angle = math.atan2(d.dot(across), d.dot(a))
            knuckle = base - a * ((base - wrist).dot(a) - mcp)
            rays.append((set(comp), knuckle, (base - wrist).dot(across), Quaternion(palm, -.9 * angle)))
            straightened.append(round(math.degrees(angle), 1))
        for i in fingers:
            if s[i] <= mcp:
                continue
            owner = next((r for r in rays if i in r[0]), None)
            if owner:
                pos[i] = owner[1] + owner[3] @ (hand[i] - owner[1])
                continue
            # Palm skin between the knuckles and the finger roots follows the nearest fingers.
            lat = (hand[i] - wrist).dot(across)
            ws = [1 / (abs(lat - r[2]) + .002) ** 3 for r in rays]
            total = sum(ws)
            ramp = smooth((s[i] - mcp) / max(best_cut - mcp, .005))
            q = sum(((r[1] + Quaternion().slerp(r[3], ramp) @ (hand[i] - r[1])) * (w / total) for r, w in zip(rays, ws)), Vector())
            pos[i] = q
    sp = {i: (pos[i] - wrist).dot(a) for i in hand}

    # 2. Curl the fingers at three knuckles. Separate fingers get joints from their own length (a pinky is
    #    much shorter than a middle finger); fused fingers and the palm curl as one row.
    def joint_set(members, own_tip):
        length = own_tip - mcp
        joints = [mcp, mcp + .45 * length, mcp + .75 * length]
        blend = [.18 * length, .1 * length, .08 * length]

        def centreline(at):
            near = [pos[i] for i in members if abs(sp[i] - at) < .004] or [pos[i] for i in fingers if abs(sp[i] - at) < .004]
            return sum(near, Vector()) / len(near)

        pivots = [centreline(j) for j in joints]
        # Some outfits already have bent fingers; only add what is missing.
        seg = [pivots[1] - pivots[0], pivots[2] - pivots[1], centreline(own_tip - .006) - pivots[2]]
        bent = [math.atan2(v.dot(palm), v.dot(a)) for v in seg]
        have = [bent[0], bent[1] - bent[0], bent[2] - bent[1]]
        turn = [max(0.0, math.radians(c) - max(0.0, h)) for c, h in zip(CURL, have)]
        return joints, blend, pivots, turn, have

    row = joint_set(fingers, tip)
    sets = {}
    for r in (rays if len(best) >= 3 else []):
        own = sorted(sp[i] for i in r[0])
        js = joint_set(r[0], own[int(len(own) * .99)])
        for i in r[0]:
            sets[i] = js
    for i in fingers:
        joints, blend, pivots, turn, _ = sets.get(i, row)
        q = pos[i]
        for j in (2, 1, 0):
            w = smooth((sp[i] - joints[j] + blend[j]) / (2 * blend[j]))
            if w > 0:
                q = pivots[j] + Quaternion(axis, turn[j] * w) @ (q - pivots[j])
        pos[i] = q
    joints, blend, _, turn, have = row

    # 3. Thumb: turn it from where it leaves the hand so its tip rests on the palm side of the curled
    #    middle finger bones; the skin at its base follows partly.
    base_pts = [hand[i] for i in thumb if s[i] < split + .006]
    tpivot = sum(base_pts, Vector()) / len(base_pts)
    far = max(thumb, key=lambda i: (hand[i] - tpivot).length)
    tdir = (hand[far] - tpivot).normalized()
    # A real thumb swings from low in the palm, not from where it leaves the hand.
    tpivot -= tdir * .015
    tdir = (hand[far] - tpivot).normalized()
    radius = sum(((hand[i] - tpivot) - tdir * (hand[i] - tpivot).dot(tdir)).length for i in thumb) / len(thumb)
    middle = [pos[i] for i in fingers if joints[1] < s[i] < joints[2]]
    rest_on = sum(middle, Vector()) / len(middle)
    rest_on += palm * radius * 2.2 + (thumb_c - centre).dot(across) * across * THUMB["toward_thumb"]
    swing = tdir.rotation_difference((rest_on - tpivot).normalized())
    swing = Quaternion().slerp(swing, THUMB["amount"])
    for i in thumb:
        pos[i] = tpivot + swing @ (hand[i] - tpivot)
    # Long (clawed) thumbs overshoot and poke out under the fist: bend the last joint back in, just enough
    # that the tip stays within the curled fingers.
    floor = max((pos[i] - wrist).dot(palm) for i in fingers)
    new_dir = (pos[far] - tpivot).normalized()
    reach = max((hand[i] - tpivot).length for i in thumb)
    knuckle_t = tpivot + new_dir * reach * .55
    flex = new_dir.cross(palm).normalized()
    thumb_bend = 0
    for deg in range(0, 95, 5):
        bent_tip = knuckle_t + Quaternion(flex, -math.radians(deg)) @ (pos[far] - knuckle_t)
        thumb_bend = deg
        if (bent_tip - wrist).dot(palm) <= floor:
            break
    if thumb_bend:
        for i in thumb:
            d = (pos[i] - tpivot).dot(new_dir)
            w = smooth((d - reach * .45) / (reach * .2))
            if w > 0:
                pos[i] = knuckle_t + Quaternion(flex, -math.radians(thumb_bend) * w) @ (pos[i] - knuckle_t)
    for i in fingers:
        if s[i] >= joints[0] - blend[0]:
            continue
        d = (hand[i] - tpivot).dot(tdir)
        off = ((hand[i] - tpivot) - tdir * d).length
        if d > -.015 and off < radius * 1.8:
            w = smooth((d + .015) / .015) * smooth(1 - (off - radius) / (radius * .8))
            pos[i] = tpivot + Quaternion().slerp(swing, w) @ (pos[i] - tpivot)

    colors = mesh.data.color_attributes.get("FistPreview")
    if colors:
        loc = lambda p: tuple(round((p - wrist).dot(e) * 100, 1) for e in (a, across, palm))
        print(f"DEBUG {side}: thumb base {loc(tpivot)} tip {loc(hand[far])} -> {loc(pos[far])}, aim {loc(rest_on)}, "
              f"thumb side {round((thumb_c - centre).dot(across) * 100, 1)}")
        for r in rays:
            t = max(r[0], key=lambda i: s[i])
            print(f"DEBUG {side}: finger tip {loc(hand[t])} -> {loc(pos[t])}")  # preview only: thumb red, separate fingers in shades of blue, the rest of the hand green
        for n, r in enumerate(rays):
            for i in r[0]:
                colors.data[i].color = (.1, .2 + .2 * n, 1, 1)
        for i in thumb:
            colors.data[i].color = (1, .15, .1, 1)
        for i in fingers:
            if not any(i in r[0] for r in rays):
                colors.data[i].color = (.3, .8, .3, 1)

    inv = W.inverted()
    moved = 0
    for i, q in pos.items():
        if (q - hand[i]).length > 1e-7:
            verts[i].co = inv @ q
            moved += 1
    return {"moved": moved, "thumb_verts": len(thumb), "thumb_split_cm": round(split * 100, 1),
            "knuckle_cm": round(mcp * 100, 1), "tip_cm": round(tip * 100, 1),
            "fingers_straightened_deg": straightened, "thumb_turn_deg": round(math.degrees(swing.angle), 1), "thumb_bend_deg": thumb_bend,
            "existing_bend_deg": [round(math.degrees(h), 1) for h in have],
            "added_bend_deg": [round(math.degrees(t), 1) for t in turn]}, (wrist, a, palm, across, tip)


def eigenvectors(m):
    # Power iteration with deflation; enough for a 3x3 covariance.
    vecs, work = [], m.copy()
    for _ in range(3):
        v = Vector((1, .5, .25))
        for _ in range(100):
            v = work @ v
            if v.length < 1e-12:
                v = Vector((0.3, 0.7, 0.1))
                break
            v.normalize()
        for u in vecs:
            v = (v - u * v.dot(u)).normalized()
        lam = v.dot(m @ v)
        work = work - lam * Matrix([[v[r] * v[c] for c in range(3)] for r in range(3)])
        vecs.append(v)
    return vecs


def render(mesh, frames, tag):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "VERTEX"
    mesh.data.color_attributes.active_color = mesh.data.color_attributes["FistPreview"]
    scene.render.resolution_x = scene.render.resolution_y = 400
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.type = "ORTHO"
    cam.data.clip_start = .001
    for side, (wrist, a, palm, across, tip) in frames.items():
        c = wrist + a * tip * .5 + palm * tip * .15
        cam.data.ortho_scale = tip * 1.2
        for name, d, up in (("back", -palm, a), ("side", across, -palm), ("palm", palm, a), ("front", a, -palm), ("thumbside", -across, -palm)):
            cam.location = c + d * (.12 if name == "front" else 1.0)
            cam.data.clip_end = .2 if name == "front" else 2.0   # keep the body behind the fist out of view
            cam.rotation_euler = Matrix((up.cross(d).normalized(), up, d)).transposed().to_euler()
            scene.render.filepath = os.path.join(render_dir, f"{tag}_{side}_{name}.png")
            bpy.ops.render.render(write_still=True)


report = {}
for name in outfits:
    source = os.path.join(game_root, clips["outfits"][name])
    folder = os.path.dirname(source)
    backup = os.path.join(folder, "openhands")
    if not render_dir and not os.path.exists(os.path.join(backup, name + ".fbx")):
        os.makedirs(backup, exist_ok=True)
        shutil.copy2(source, os.path.join(backup, name + ".fbx"))
    original = os.path.join(backup, name + ".fbx")

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=original if os.path.exists(original) else source)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    mesh = next(o for o in bpy.data.objects if o.type == "MESH")
    adj = {}
    for e in mesh.data.edges:
        x, y = e.vertices
        adj.setdefault(x, []).append(y)
        adj.setdefault(y, []).append(x)
    frames, report[name] = {}, {}
    if render_dir:
        col = mesh.data.color_attributes.new("FistPreview", "FLOAT_COLOR", "POINT")
        for c in col.data:
            c.color = (.7, .7, .7, 1)
    for side in ("Left", "Right"):
        report[name][side], frames[side] = fist(mesh, arm, side, adj)
    mesh.data.update()
    if render_dir:
        render(mesh, frames, name)
        print(f"FIST PREVIEW {name}: {report[name]}")
        continue

    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(filepath=source, use_selection=True, object_types={"ARMATURE", "MESH"},
                             bake_anim=False, add_leaf_bones=False, axis_forward="-Y", axis_up="Z")
    print(f"FIST {name}: {report[name]}")

json.dump(report, open(os.path.join(game_root, "Animation Testing", "fist_report.json"), "w"), indent=1)
print("FIST HANDS DONE")
