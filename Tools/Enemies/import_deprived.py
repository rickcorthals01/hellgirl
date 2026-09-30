# Unreal (run through deprived.ps1): imports the rigged Deprived as /Game/Enemies/Deprived/Deprived on Hellgirl's own
# skeleton (/Game/Hellgirl/Outfits/Rags/Rags_Skeleton, left untouched), so it plays her Rags clips directly; with its
# Meshy textures (2048 copies made by deprived.ps1), material M_Deprived and a physics asset for the death ragdoll.
import os, sys
import unreal as u

folder = os.environ["HELLGIRL_DEPRIVED_FOLDER"]   # Deprived.fbx and the 2048 textures
DEST = "/Game/Enemies/Deprived"
SKELETON = "/Game/Hellgirl/Outfits/Rags/Rags_Skeleton"
tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary
u.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")


def run(task):
    tools.import_asset_tasks([task])
    return list(task.get_objects())


# --- Textures -----------------------------------------------------------------------------------------------------
textures = {}
for key, srgb, kind in (("base", True, None), ("normal", False, "normal"), ("metallic", False, "mask"), ("roughness", False, "mask")):
    task = u.AssetImportTask()
    task.filename = os.path.join(folder, f"Deprived_{key}.png")
    task.destination_path = DEST
    task.destination_name = f"Deprived_{key}"
    task.automated = True; task.save = True; task.replace_existing = True
    tex = next(o for o in run(task) if isinstance(o, u.Texture2D))
    tex.set_editor_property("srgb", srgb)
    if kind == "normal":
        tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_NORMALMAP)
    elif kind == "mask":
        tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_MASKS)
    u.EditorAssetLibrary.save_loaded_asset(tex, False)
    textures[key] = tex

# --- Material: its own dark skin and hair, with a violet rim of light at its edges so it shows in the maze's dark -----
path = f"{DEST}/M_Deprived"
if u.EditorAssetLibrary.does_asset_exist(path):
    material = u.load_asset(path)
    lib.delete_all_material_expressions(material)
else:
    material = tools.create_asset("M_Deprived", DEST, u.Material, u.MaterialFactoryNew())


def sample(key, y, sampler=None):
    node = lib.create_material_expression(material, u.MaterialExpressionTextureSample, -500, y)
    node.set_editor_property("texture", textures[key])
    if sampler:
        node.set_editor_property("sampler_type", sampler)
    return node


def scalar(name, y, value):
    node = lib.create_material_expression(material, u.MaterialExpressionScalarParameter, -700, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


lib.connect_material_property(sample("base", -300), "RGB", u.MaterialProperty.MP_BASE_COLOR)
lib.connect_material_property(sample("normal", -60, u.MaterialSamplerType.SAMPLERTYPE_NORMAL), "RGB", u.MaterialProperty.MP_NORMAL)
lib.connect_material_property(sample("metallic", 180, u.MaterialSamplerType.SAMPLERTYPE_MASKS), "R", u.MaterialProperty.MP_METALLIC)
lib.connect_material_property(sample("roughness", 420, u.MaterialSamplerType.SAMPLERTYPE_MASKS), "R", u.MaterialProperty.MP_ROUGHNESS)
edge = lib.create_material_expression(material, u.MaterialExpressionFresnel, -500, 660)
edge.set_editor_property("base_reflect_fraction", 0.0)
lib.connect_material_expressions(scalar("RimPower", 660, 2.5), "", edge, "ExponentIn")
tint = lib.create_material_expression(material, u.MaterialExpressionVectorParameter, -700, 780)
tint.set_editor_property("parameter_name", "RimTint")
tint.set_editor_property("default_value", u.LinearColor(.5, .15, .9, 1))
coloured = lib.create_material_expression(material, u.MaterialExpressionMultiply, -300, 700)
lib.connect_material_expressions(edge, "", coloured, "A")
lib.connect_material_expressions(tint, "", coloured, "B")
rim = lib.create_material_expression(material, u.MaterialExpressionMultiply, -150, 700)
lib.connect_material_expressions(coloured, "", rim, "A")
lib.connect_material_expressions(scalar("Rim", 900, 1.2), "", rim, "B")
lib.connect_material_property(rim, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
material.set_editor_property("two_sided", True)   # thin hair locks, and the few faces cut where hair was fused to the arms
material.set_editor_property("used_with_skeletal_mesh", True)
lib.recompile_material(material)
u.EditorAssetLibrary.save_loaded_asset(material, False)

# --- Skeletal mesh, on Hellgirl's skeleton ---------------------------------------------------------------------------
skeleton = u.load_asset(SKELETON)
if not skeleton:
    u.log_error("DEPRIVED IMPORT FAILED: no " + SKELETON)
    sys.exit(1)
opt = u.FbxImportUI()
opt.automated_import_should_detect_type = False
opt.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
opt.import_as_skeletal = True
opt.import_mesh = True
opt.import_animations = False
opt.import_materials = False
opt.import_textures = False
opt.create_physics_asset = True
opt.skeleton = skeleton   # her reference pose is kept: the rig is her own T-pose
task = u.AssetImportTask()
task.filename = os.path.join(folder, "Deprived.fbx")
task.destination_path = DEST
task.destination_name = "Deprived"
task.automated = True; task.save = False; task.replace_existing = True
task.options = opt; task.factory = u.FbxFactory()
mesh = next((o for o in run(task) if isinstance(o, u.SkeletalMesh)), None)
if not mesh or mesh.get_editor_property("skeleton") != skeleton:
    u.log_error("DEPRIVED IMPORT FAILED: mesh (or not on Hellgirl's skeleton)")
    sys.exit(1)
slots = mesh.get_editor_property("materials")
for i in range(len(slots)):
    slot = slots[i]
    slot.material_interface = material
    slots[i] = slot
mesh.set_editor_property("materials", slots)
physics = mesh.get_editor_property("physics_asset")
if not physics:
    try:
        physics = u.SkeletalMeshEditorSubsystem.create_physics_asset(mesh, True)
    except Exception:
        physics = u.EditorSkeletalMeshLibrary.create_physics_asset(mesh)
        if physics:
            mesh.set_editor_property("physics_asset", physics)
if not physics:
    u.log_error("DEPRIVED IMPORT FAILED: no physics asset")
    sys.exit(1)
u.EditorAssetLibrary.save_loaded_asset(mesh, False)
u.EditorAssetLibrary.save_loaded_asset(physics, False)
u.log(f"DEPRIVED IMPORT PASSED: mesh ({mesh.get_path_name()}), material, physics asset, on {SKELETON}")
