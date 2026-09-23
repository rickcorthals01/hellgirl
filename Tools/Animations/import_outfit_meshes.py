# Unreal (run through flatten_feet.ps1): re-imports re-posed outfit meshes over their existing assets,
# updating each outfit's skeleton to the new reference pose and keeping its material and physics asset.
import os, sys
import unreal as u

game_root = os.environ["HELLGIRL_GAME_ROOT"]
outfits = [o for o in os.environ["HELLGIRL_OUTFITS"].split(",") if o]
tools = u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
failed = []
for name in outfits:
    dest = f"/Game/Hellgirl/Outfits/{name}"
    old = u.load_asset(f"{dest}/{name}")
    skeleton = old.get_editor_property("skeleton")
    physics = old.get_editor_property("physics_asset")
    material = u.load_asset(f"{dest}/M_{name}")
    opt = u.FbxImportUI()
    opt.automated_import_should_detect_type = False
    opt.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
    opt.import_as_skeletal = True
    opt.import_mesh = True
    opt.import_animations = False
    opt.import_materials = False
    opt.import_textures = False
    opt.create_physics_asset = False
    opt.skeleton = skeleton
    opt.skeletal_mesh_import_data.set_editor_property("update_skeleton_reference_pose", True)
    task = u.AssetImportTask()
    task.filename = os.path.join(game_root, "Animation Testing", "ExtraSkins", name, name + ".fbx")
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = opt
    task.factory = u.FbxFactory()
    tools.import_asset_tasks([task])
    mesh = next((o for o in task.get_objects() if isinstance(o, u.SkeletalMesh)), None)
    if not mesh or mesh.get_editor_property("skeleton") != skeleton:
        failed.append(name)
        continue
    slots = mesh.get_editor_property("materials")
    for i in range(len(slots)):
        slot = slots[i]
        slot.material_interface = material
        slots[i] = slot
    mesh.set_editor_property("materials", slots)
    if physics:
        mesh.set_editor_property("physics_asset", physics)
    u.EditorAssetLibrary.save_loaded_asset(mesh, False)
    u.EditorAssetLibrary.save_loaded_asset(skeleton, False)
if failed:
    u.log_error("OUTFIT MESH IMPORT FAILED: " + ", ".join(failed))
    sys.exit(1)
u.log(f"OUTFIT MESH IMPORT PASSED: {', '.join(outfits)}")
