# Unreal (run through rig_mini_succubus.ps1): imports the rigged mini succubus as /Game/Enemies/MiniSuccubus/
# MiniSuccubus (own skeleton and physics asset), her Meshy textures and material, and the flying clips from
# mini_succubus_flight.py as /Game/Enemies/MiniSuccubus/Animations/<Clip>.
import os, sys
import unreal as u

game_root = os.environ["HELLGIRL_GAME_ROOT"]
folder = os.path.join(game_root, r"Animation Testing\Enemies\MiniSuccubus")
textures_dir = os.path.join(game_root, r"Meshy Models\Enemies\Mini Succubus Model\mini_succubus_enemy_m\Meshy_AI_mini_succubus_enemy_m_0925130025_image-to-3d-texture_fbx")
stem = "Meshy_AI_mini_succubus_enemy_m_0925130025_image-to-3d-texture"
DEST = "/Game/Enemies/MiniSuccubus"
tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary
u.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")


def run(task):
    tools.import_asset_tasks([task])
    return list(task.get_objects())


# --- Textures -----------------------------------------------------------------------------------------------------
textures = {}
for key, suffix, srgb, kind in (("base", "", True, None), ("normal", "_normal", False, "normal"), ("emission", "_emission", True, None),
                                ("metal", "_metallic", False, "mask"), ("rough", "_roughness", False, "mask")):
    task = u.AssetImportTask()
    task.filename = os.path.join(textures_dir, stem + suffix + ".png")
    task.destination_path = DEST
    task.destination_name = f"MiniSuccubus_{key}"
    task.automated = True; task.save = True; task.replace_existing = True
    tex = next(o for o in run(task) if isinstance(o, u.Texture2D))
    tex.set_editor_property("srgb", srgb)
    if kind == "normal":
        tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_NORMALMAP)
    elif kind == "mask":
        tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_MASKS)
    u.EditorAssetLibrary.save_loaded_asset(tex, False)
    textures[key] = tex

# --- Material -----------------------------------------------------------------------------------------------------
path = f"{DEST}/M_MiniSuccubus"
if u.EditorAssetLibrary.does_asset_exist(path):
    material = u.load_asset(path)
    lib.delete_all_material_expressions(material)
else:
    material = tools.create_asset("M_MiniSuccubus", DEST, u.Material, u.MaterialFactoryNew())


def sample(key, y, sampler=None):
    node = lib.create_material_expression(material, u.MaterialExpressionTextureSample, -500, y)
    node.set_editor_property("texture", textures[key])
    if sampler:
        node.set_editor_property("sampler_type", sampler)
    return node


lib.connect_material_property(sample("base", -300), "RGB", u.MaterialProperty.MP_BASE_COLOR)
lib.connect_material_property(sample("normal", -60, u.MaterialSamplerType.SAMPLERTYPE_NORMAL), "RGB", u.MaterialProperty.MP_NORMAL)
lib.connect_material_property(sample("metal", 180, u.MaterialSamplerType.SAMPLERTYPE_MASKS), "R", u.MaterialProperty.MP_METALLIC)
lib.connect_material_property(sample("rough", 420, u.MaterialSamplerType.SAMPLERTYPE_MASKS), "R", u.MaterialProperty.MP_ROUGHNESS)
glow = lib.create_material_expression(material, u.MaterialExpressionMultiply, -200, 660)
lib.connect_material_expressions(sample("emission", 660), "RGB", glow, "A")
strength = lib.create_material_expression(material, u.MaterialExpressionScalarParameter, -500, 900)
strength.set_editor_property("parameter_name", "EmissiveStrength")
strength.set_editor_property("default_value", 2.0)
lib.connect_material_expressions(strength, "", glow, "B")
lib.connect_material_property(glow, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
material.set_editor_property("two_sided", True)  # thin wing membrane
lib.recompile_material(material)
u.EditorAssetLibrary.save_loaded_asset(material, False)

# --- Skeletal mesh --------------------------------------------------------------------------------------------------
opt = u.FbxImportUI()
opt.automated_import_should_detect_type = False
opt.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
opt.import_as_skeletal = True
opt.import_mesh = True
opt.import_animations = False
opt.import_materials = False
opt.import_textures = False
opt.create_physics_asset = True
existing = f"{DEST}/MiniSuccubus"
if u.EditorAssetLibrary.does_asset_exist(existing):
    opt.skeleton = u.load_asset(existing).get_editor_property("skeleton")
    opt.skeletal_mesh_import_data.set_editor_property("update_skeleton_reference_pose", True)
task = u.AssetImportTask()
task.filename = os.path.join(folder, "MiniSuccubus.fbx")
task.destination_path = DEST
task.destination_name = "MiniSuccubus"
task.automated = True; task.save = False; task.replace_existing = True
task.options = opt; task.factory = u.FbxFactory()
mesh = next((o for o in run(task) if isinstance(o, u.SkeletalMesh)), None)
if not mesh:
    u.log_error("MINI SUCCUBUS IMPORT FAILED: mesh")
    sys.exit(1)
slots = mesh.get_editor_property("materials")
for i in range(len(slots)):
    slot = slots[i]
    slot.material_interface = material
    slots[i] = slot
mesh.set_editor_property("materials", slots)
skeleton = mesh.get_editor_property("skeleton")
physics = mesh.get_editor_property("physics_asset")  # used by ragdoll deaths
if not physics:
    # The automated FBX import does not build one: make it from the mesh (default bodies per bone) and assign it.
    try:
        physics = u.SkeletalMeshEditorSubsystem.create_physics_asset(mesh, True)
    except Exception:
        physics = u.EditorSkeletalMeshLibrary.create_physics_asset(mesh)
        if physics:
            mesh.set_editor_property("physics_asset", physics)
if not physics:
    u.log_error("MINI SUCCUBUS IMPORT FAILED: no physics asset")
    sys.exit(1)
u.EditorAssetLibrary.save_loaded_asset(mesh, False)
u.EditorAssetLibrary.save_loaded_asset(skeleton, False)
u.EditorAssetLibrary.save_loaded_asset(physics, False)

# --- Clips ------------------------------------------------------------------------------------------------------------
clips_dir = os.path.join(folder, "Clips", "MiniSuccubus")
done, failed = [], []
for file in sorted(os.listdir(clips_dir)):
    if not file.endswith(".fbx"):
        continue
    clip = file[:-4]
    opt = u.FbxImportUI()
    opt.automated_import_should_detect_type = False
    opt.mesh_type_to_import = u.FBXImportType.FBXIT_ANIMATION
    opt.import_mesh = False
    opt.import_animations = True
    opt.import_materials = False
    opt.import_textures = False
    opt.skeleton = skeleton
    opt.anim_sequence_import_data.set_editor_property("use_default_sample_rate", False)
    opt.anim_sequence_import_data.set_editor_property("custom_sample_rate", 60)
    task = u.AssetImportTask()
    task.filename = os.path.join(clips_dir, file)
    task.destination_path = DEST + "/Animations"
    task.destination_name = clip
    task.automated = True; task.save = True; task.replace_existing = True
    task.options = opt; task.factory = u.FbxFactory()
    anims = [o for o in run(task) if isinstance(o, u.AnimSequence)]
    (done if len(anims) == 1 and anims[0].get_editor_property("skeleton") == skeleton else failed).append(clip)
if failed:
    u.log_error("MINI SUCCUBUS IMPORT FAILED: clips " + ", ".join(failed))
    sys.exit(1)
# Clips she no longer has (e.g. the ground walk and run from before she was flight-only) are removed.
for path in u.EditorAssetLibrary.list_assets(DEST + "/Animations", recursive=False):
    if path.split(".")[-1] not in done:
        u.EditorAssetLibrary.delete_asset(path)
u.log(f"MINI SUCCUBUS IMPORT PASSED: mesh, material, {len(done)} clips ({', '.join(done)})")
