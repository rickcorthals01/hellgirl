# Blender: some Meshy outfits were generated standing on tiptoe (heel several cm above the floor at rest).
# This rotates each foot flat at the ankle (toes keep their orientation), bakes that into the mesh as the
# new rest pose, lowers the body so the soles touch the floor, and exports <Outfit>.fbx.
# The untouched originals are kept in <outfit folder>/tiptoe/ and are always the input, so re-running is safe.
# Run through flatten_feet.ps1, or:
#   blender -b --factory-startup -P flatten_feet.py -- <game root> <Outfit,Outfit>
import bpy, json, math, os, shutil, sys
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
game_root, outfits = args[0], args[1].split(",")
report = {}


def sole_heights(arm, mesh, evaluated):
    """Lowest point under toes/ball/arch/heel of each foot, in cm above the floor (z = 0)."""
    if evaluated:
        ev = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        me = ev.to_mesh()
        pts = [mesh.matrix_world @ v.co for v in me.vertices]
        ev.to_mesh_clear()
    else:
        pts = [mesh.matrix_world @ v.co for v in mesh.data.vertices]
    result = {}
    for side in ("Left", "Right"):
        ankle = arm.matrix_world @ arm.pose.bones[side + "Foot"].head
        ball = arm.matrix_world @ arm.pose.bones[side + "ToeBase"].head
        along = (ball - ankle).to_2d().normalized()
        mid = ((ankle + ball) / 2).to_2d()
        foot = [p for p in pts if p.z < ankle.z + .03 and (p.to_2d() - mid).length < .14]
        d = [p.to_2d().dot(along) for p in foot]
        lo, hi = min(d), max(d)
        def lowest(a, b):
            return min((p for p, x in zip(foot, d) if a <= 1 - (x - lo) / (hi - lo) <= b), key=lambda p: p.z)
        result[side] = {k: lowest(a, b) for k, (a, b) in {"toes": (0, .15), "ball": (.2, .35), "heel": (.8, 1)}.items()}
    return result


def flatten(name):
    folder = os.path.join(game_root, "Animation Testing", "ExtraSkins", name)
    backup = os.path.join(folder, "tiptoe")
    os.makedirs(backup, exist_ok=True)
    for f in (name + ".fbx", "Walk.fbx", "Run.fbx", "NeutralIdle.fbx"):
        if not os.path.exists(os.path.join(backup, f)):
            shutil.copy2(os.path.join(folder, f), os.path.join(backup, f))

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.join(backup, name + ".fbx"))
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    mesh = next(o for o in bpy.data.objects if o.type == "MESH")
    for pb in arm.pose.bones:
        pb.matrix_basis.identity()
    bpy.context.view_layer.update()
    before = sole_heights(arm, mesh, False)
    W = arm.matrix_world.to_3x3().normalized()
    angles = {}
    for side in ("Left", "Right"):
        foot, toe = arm.pose.bones[side + "Foot"], arm.pose.bones[side + "ToeBase"]
        heel, ball = before[side]["heel"], before[side]["ball"]
        along = (ball - heel).to_2d().normalized()
        axis = W.inverted() @ Vector((along.y, -along.x, 0.0))  # across the foot, in armature space

        def pose(theta, phi):
            """Rotate the foot by theta at the ankle; the toes keep their rest orientation, plus phi at the ball."""
            foot.matrix_basis.identity(); toe.matrix_basis.identity()
            bpy.context.view_layer.update()
            pivot = foot.matrix.translation.copy()
            foot.matrix = Matrix.Translation(pivot) @ Matrix.Rotation(theta, 4, axis) @ Matrix.Translation(-pivot) @ foot.matrix
            bpy.context.view_layer.update()
            now = toe.matrix
            level = Matrix.LocRotScale(now.translation, toe.bone.matrix_local.to_quaternion(), now.to_scale())
            toe.matrix = Matrix.Translation(now.translation) @ Matrix.Rotation(phi, 4, axis) @ Matrix.Translation(-now.translation) @ level
            bpy.context.view_layer.update()

        def solve(gap, lo, hi, steps=22, fallback=None):
            if gap(lo) * gap(hi) > 0:  # no angle in range closes the gap
                return fallback if fallback is not None else (lo if abs(gap(lo)) < abs(gap(hi)) else hi)
            for _ in range(steps):
                m = (lo + hi) / 2
                lo, hi = (lo, m) if gap(lo) * gap(m) <= 0 else (m, hi)
            return (lo + hi) / 2

        def heights(theta, phi):
            pose(theta, phi)
            s = sole_heights(arm, mesh, True)[side]
            return {k: p.z for k, p in s.items()}

        # Measured on the deformed mesh, not a rigid foot: heel level with the ball, then toes level too.
        theta = solve(lambda t: (lambda h: h["heel"] - h["ball"])(heights(t, 0.0)), 0.0, math.radians(60))
        # Toes are mostly weighted to the foot bone on these models; if turning the toe bone cannot
        # level them, leave it alone rather than distort the toes.
        phi = solve(lambda p: (lambda h: h["toes"] - h["ball"])(heights(theta, p)), math.radians(-40), math.radians(40), fallback=0.0)
        pose(theta, phi)
        angles[side] = {"foot": round(math.degrees(theta), 1), "toes": round(math.degrees(phi), 1)}
    # Bake the flattened feet into the mesh, then make this pose the armature's rest pose.
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = mesh
    mesh.select_set(True)
    modifier = next(m for m in mesh.modifiers if m.type == "ARMATURE")
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    new_mod = mesh.modifiers.new("Armature", "ARMATURE")
    new_mod.object = arm

    # Lower everything so the lowest point of the flat soles sits on the floor.
    drop = min((mesh.matrix_world @ v.co).z for v in mesh.data.vertices)
    shift = Matrix.Translation((0, 0, -drop))
    mesh.data.transform(mesh.matrix_world.inverted() @ shift @ mesh.matrix_world)
    arm.data.transform(arm.matrix_world.inverted() @ shift @ arm.matrix_world)
    bpy.context.view_layer.update()
    after = sole_heights(arm, mesh, True)

    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(filepath=os.path.join(folder, name + ".fbx"), use_selection=True, object_types={"ARMATURE", "MESH"},
                             bake_anim=False, add_leaf_bones=False, axis_forward="-Y", axis_up="Z")
    cm = lambda s: {k: round(p.z * 100, 1) for k, p in s.items()}
    report[name] = {"foot_rotation_deg": angles, "lowered_cm": round(drop * 100, 1),
                    "before_cm": {s: cm(v) for s, v in before.items()}, "after_cm": {s: cm(v) for s, v in after.items()},
                    "vertices": len(mesh.data.vertices)}
    print(f"FLATTENED {name}: {report[name]}")


for outfit in outfits:
    flatten(outfit)
json.dump(report, open(os.path.join(game_root, "Animation Testing", "ExtraSkins", "flatten_report.json"), "w"), indent=1)
print("FLATTEN FEET DONE")
