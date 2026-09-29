# Unreal (run through import_weapons.ps1): Hellgirl's swords from "Hellgirl Game/Weapons" into /Game/Weapons.
#   SM_BatSword      her sword: one atlas (base colour, roughness, metallic) on every slot.
#   SM_InfernalSword the upgrade: its glowing blade (base colour, roughness, emissive) on slot 0; the steel and
#                    leather slots keep the colours imported from the FBX.
# Both are 155 cm long with the pivot at the grip's centre and the blade along +Z (the fighter scales and turns them).
import os, sys
import unreal as u

SOURCE = os.environ["HELLGIRL_WEAPON_SOURCE"]
ROOT = "/Game/Weapons"
tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary
u.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
failed = []


def texture(folder, file, name, srgb):
    task = u.AssetImportTask()
    task.filename = os.path.join(SOURCE, folder, file)
    task.destination_path = f"{ROOT}/Textures"
    task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False
    tools.import_asset_tasks([task])
    tex = u.load_asset(f"{ROOT}/Textures/{name}")
    if not tex:
        failed.append(name)
        return None
    tex.set_editor_property("srgb", srgb)
    if not srgb:
        tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_MASKS)
    u.EditorAssetLibrary.save_loaded_asset(tex, False)
    return tex


def node(material, cls, x, y, **props):
    expression = lib.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


# --- M_Weapon: base colour, roughness and metallic maps, and an emissive map times Glow -------------------------------
path = f"{ROOT}/M_Weapon"
if u.EditorAssetLibrary.does_asset_exist(path):
    m = u.load_asset(path)
    lib.delete_all_material_expressions(m)
else:
    m = tools.create_asset("M_Weapon", ROOT, u.Material, u.MaterialFactoryNew())
black = u.load_asset("/Engine/EngineResources/Black")
white = u.load_asset("/Engine/EngineResources/WhiteSquareTexture")


def sample(name, x, y, default, masks):
    return node(m, u.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, texture=default,
                sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS if masks else u.MaterialSamplerType.SAMPLERTYPE_COLOR)


base = sample("BaseColor", -700, -300, white, False)
rough = sample("Roughness", -700, 0, white, True)
metal = sample("Metallic", -700, 300, black, True)
glow_map = sample("Emissive", -700, 600, black, False)
metal_scale = node(m, u.MaterialExpressionScalarParameter, -700, 500, parameter_name="MetallicScale", default_value=1.0)
glow = node(m, u.MaterialExpressionScalarParameter, -700, 850, parameter_name="Glow", default_value=0.0)
metal_mul = node(m, u.MaterialExpressionMultiply, -350, 300)
lib.connect_material_expressions(metal, "R", metal_mul, "A")
lib.connect_material_expressions(metal_scale, "", metal_mul, "B")
glow_mul = node(m, u.MaterialExpressionMultiply, -350, 600)
lib.connect_material_expressions(glow_map, "RGB", glow_mul, "A")
lib.connect_material_expressions(glow, "", glow_mul, "B")
lib.connect_material_property(base, "RGB", u.MaterialProperty.MP_BASE_COLOR)
lib.connect_material_property(rough, "R", u.MaterialProperty.MP_ROUGHNESS)
lib.connect_material_property(metal_mul, "", u.MaterialProperty.MP_METALLIC)
lib.connect_material_property(glow_mul, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(m)
u.EditorAssetLibrary.save_loaded_asset(m, False)


def instance(name, textures, scalars):
    p = f"{ROOT}/{name}"
    mi = u.load_asset(p) if u.EditorAssetLibrary.does_asset_exist(p) else \
        tools.create_asset(name, ROOT, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", m)
    for param, tex in textures.items():
        if tex: lib.set_material_instance_texture_parameter_value(mi, param, tex)
    for param, value in scalars.items():
        lib.set_material_instance_scalar_parameter_value(mi, param, value)
    u.EditorAssetLibrary.save_loaded_asset(mi, False)
    return mi


bat = instance("MI_BatSword", {
    "BaseColor": texture("Bat Sword", "BatSword_BaseColor.png", "T_BatSword_D", True),
    "Roughness": texture("Bat Sword", "BatSword_Roughness.png", "T_BatSword_R", False),
    "Metallic": texture("Bat Sword", "BatSword_Metallic.png", "T_BatSword_M", False)}, {})
# The README's blade: metallic 0.7 (a white map scaled), emissive map times 2.5.
infernal = instance("MI_InfernalBlade", {
    "BaseColor": texture("Infernal Sword", "Sword_Blade_BaseColor.png", "T_InfernalBlade_D", True),
    "Roughness": texture("Infernal Sword", "Sword_Blade_Roughness.png", "T_InfernalBlade_R", False),
    "Metallic": white,
    "Emissive": texture("Infernal Sword", "Sword_Blade_Emissive.png", "T_InfernalBlade_E", True)},
    {"MetallicScale": 0.7, "Glow": 2.5})


def mesh(folder, file, name, own_materials):
    opt = u.FbxImportUI()
    opt.automated_import_should_detect_type = False
    opt.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    opt.import_mesh = True
    opt.import_materials = own_materials
    opt.import_textures = False
    opt.import_as_skeletal = False
    data = opt.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("normal_import_method", u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    task = u.AssetImportTask()
    task.filename = os.path.join(SOURCE, folder, file)
    task.destination_path = ROOT
    task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False
    task.options = opt; task.factory = u.FbxFactory()
    tools.import_asset_tasks([task])
    result = next((o for o in task.get_objects() if isinstance(o, u.StaticMesh)), None)
    if not result: failed.append(name)
    return result


sword = mesh("Bat Sword", "Bat_Sword.fbx", "SM_BatSword", False)
if sword:
    slots = sword.get_editor_property("static_materials")
    for i in range(len(slots)):
        slot = slots[i]; slot.material_interface = bat; slots[i] = slot
    sword.set_editor_property("static_materials", slots)
    u.EditorAssetLibrary.save_loaded_asset(sword, False)
    u.log(f"SM_BatSword: {len(slots)} slots, bounds {sword.get_bounding_box()}")

upgrade = mesh("Infernal Sword", "Infernal_Sword.fbx", "SM_InfernalSword", True)
if upgrade:
    slots = upgrade.get_editor_property("static_materials")
    names = []
    for i in range(len(slots)):
        slot = slots[i]
        names.append(str(slot.material_slot_name))
        if "Blade" in str(slot.material_slot_name): slot.material_interface = infernal
        slots[i] = slot
    if not any("Blade" in n for n in names): failed.append("SM_InfernalSword blade slot (" + ", ".join(names) + ")")
    upgrade.set_editor_property("static_materials", slots)
    u.EditorAssetLibrary.save_loaded_asset(upgrade, False)
    # The FBX's own blade material and the textures it pulled out of the file are replaced by MI_InfernalBlade.
    for extra in ("01_Infernal_Blade", "Sword_Blade_BaseColor", "Sword_Blade_Emissive", "Sword_Blade_Roughness"):
        if u.EditorAssetLibrary.does_asset_exist(f"{ROOT}/{extra}"): u.EditorAssetLibrary.delete_asset(f"{ROOT}/{extra}")
    for asset in u.EditorAssetLibrary.list_assets(ROOT, recursive=False):
        u.EditorAssetLibrary.save_asset(asset, False)
    u.log(f"SM_InfernalSword: slots {names}, bounds {upgrade.get_bounding_box()}")

if failed:
    u.log_error("WEAPON IMPORT FAILED: " + ", ".join(failed))
    sys.exit(1)
u.log("WEAPON IMPORT PASSED")
