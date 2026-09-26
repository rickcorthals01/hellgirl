# Unreal (run through upgrade_outfit.ps1): replaces an outfit's textures with a new Meshy texture set, in place, so
# M_<Outfit> keeps pointing at them: <stem>.png -> <Outfit>_base, _metallic -> _metal, _roughness -> _rough, and adds
# _normal -> <Outfit>_normal, wired into the material's normal input if the material had none.
import os, sys
import unreal as u

outfit = os.environ["HELLGIRL_OUTFIT"]
folder = os.environ["HELLGIRL_TEXTURE_DIR"]
stem = os.environ["HELLGIRL_TEXTURE_STEM"]
dest = f"/Game/Hellgirl/Outfits/{outfit}"
tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary
failed = []
textures = {}
for suffix, name, kind in (("", "base", "color"), ("_metallic", "metal", "mask"), ("_roughness", "rough", "mask"), ("_normal", "normal", "normal")):
    path = os.path.join(folder, stem + suffix + ".png")
    if not os.path.exists(path):
        failed.append(path)
        continue
    task = u.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.destination_name = f"{outfit}_{name}"
    task.automated = True; task.save = True; task.replace_existing = True
    tools.import_asset_tasks([task])
    tex = next((o for o in task.get_objects() if isinstance(o, u.Texture2D)), None)
    if not tex:
        failed.append(name)
        continue
    tex.set_editor_property("srgb", kind == "color")
    if kind == "normal":
        tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_NORMALMAP)
    elif kind == "mask":
        tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property("max_texture_size", 4096)
    u.EditorAssetLibrary.save_loaded_asset(tex, False)
    textures[name] = tex
    u.log(f"TEXTURE {outfit}_{name}: {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}")

material = u.load_asset(f"{dest}/M_{outfit}")
if material:
    if "normal" in textures and not lib.get_material_property_input_node(material, u.MaterialProperty.MP_NORMAL):
        node = lib.create_material_expression(material, u.MaterialExpressionTextureSample, -600, 400)
        node.set_editor_property("texture", textures["normal"])
        lib.connect_material_property(node, "RGB", u.MaterialProperty.MP_NORMAL)
        u.log(f"TEXTURE M_{outfit}: normal map wired")
    # Each sampler must match its texture's type (a colour sampler on a mask texture fails to compile).
    kinds = {u.TextureCompressionSettings.TC_NORMALMAP: u.MaterialSamplerType.SAMPLERTYPE_NORMAL,
             u.TextureCompressionSettings.TC_MASKS: u.MaterialSamplerType.SAMPLERTYPE_MASKS}
    for prop in (u.MaterialProperty.MP_BASE_COLOR, u.MaterialProperty.MP_METALLIC, u.MaterialProperty.MP_ROUGHNESS, u.MaterialProperty.MP_NORMAL):
        node = lib.get_material_property_input_node(material, prop)
        tex = node.get_editor_property("texture") if node and hasattr(node, "texture") else None
        if tex:
            node.set_editor_property("sampler_type", kinds.get(tex.get_editor_property("compression_settings"), u.MaterialSamplerType.SAMPLERTYPE_COLOR))
    lib.recompile_material(material)
    u.EditorAssetLibrary.save_loaded_asset(material, False)
if failed:
    u.log_error("OUTFIT TEXTURES FAILED: " + ", ".join(failed))
    sys.exit(1)
u.log(f"OUTFIT TEXTURES PASSED: {outfit}")
