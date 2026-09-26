# Unreal (run through swamp_enemies.ps1): imports the swamp enemies (World II) as /Game/Enemies/Swamp/<Name>/<Name>:
# the rigged Meshy model (own skeleton, physics asset for the death ragdoll), its textures and material M_<Name>, and
# the clips prepare_clips.py fitted onto it (Animations/<Clip>).
import json, os, sys
import unreal as u

game_root = os.environ["HELLGIRL_GAME_ROOT"]
clip_root = os.environ["HELLGIRL_CLIP_DIR"]
enemies = json.loads(os.environ["HELLGIRL_ENEMIES"])   # {"Rat": "<model fbx relative to the game root>", ...}
tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary
u.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
failed, done = [], []


def run(task):
    tools.import_asset_tasks([task])
    return list(task.get_objects())


for name, rel in enemies.items():
    dest = f"/Game/Enemies/Swamp/{name}"
    model = os.path.join(game_root, rel)
    folder = os.path.dirname(model)
    # Textures: base colour, metallic and roughness (Meshy's texture set).
    textures = {}
    for suffix, key, kind in (("", "base", "color"), ("_metallic", "metal", "mask"), ("_roughness", "rough", "mask")):
        task = u.AssetImportTask()
        task.filename = os.path.join(folder, f"Meshy_AI_texture_0{suffix}.png")
        task.destination_path = dest
        task.destination_name = f"{name}_{key}"
        task.automated = True; task.save = True; task.replace_existing = True
        tex = next((o for o in run(task) if isinstance(o, u.Texture2D)), None)
        if not tex:
            failed.append(f"{name} texture {key}")
            continue
        tex.set_editor_property("srgb", kind == "color")
        if kind == "mask":
            tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_MASKS)
        u.EditorAssetLibrary.save_loaded_asset(tex, False)
        textures[key] = tex
    # Material: the samplers match their textures' types (a colour sampler on a mask texture fails to compile).
    path = f"{dest}/M_{name}"
    if u.EditorAssetLibrary.does_asset_exist(path):
        material = u.load_asset(path)
        lib.delete_all_material_expressions(material)
    else:
        material = tools.create_asset(f"M_{name}", dest, u.Material, u.MaterialFactoryNew())
    material.set_editor_property("used_with_skeletal_mesh", True)
    for key, prop, sampler, y in (("base", u.MaterialProperty.MP_BASE_COLOR, u.MaterialSamplerType.SAMPLERTYPE_COLOR, -200),
                                  ("metal", u.MaterialProperty.MP_METALLIC, u.MaterialSamplerType.SAMPLERTYPE_MASKS, 100),
                                  ("rough", u.MaterialProperty.MP_ROUGHNESS, u.MaterialSamplerType.SAMPLERTYPE_MASKS, 400)):
        if key not in textures:
            continue
        node = lib.create_material_expression(material, u.MaterialExpressionTextureSample, -500, y)
        node.set_editor_property("texture", textures[key])
        node.set_editor_property("sampler_type", sampler)
        lib.connect_material_property(node, "RGB" if key == "base" else "R", prop)
    lib.recompile_material(material)
    u.EditorAssetLibrary.save_loaded_asset(material, False)

    # The rigged model, on its own skeleton (kept when re-importing).
    opt = u.FbxImportUI()
    opt.automated_import_should_detect_type = False
    opt.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
    opt.import_as_skeletal = True
    opt.import_mesh = True
    opt.import_animations = False
    opt.import_materials = False
    opt.import_textures = False
    opt.create_physics_asset = True
    existing = f"{dest}/{name}"
    if u.EditorAssetLibrary.does_asset_exist(existing):
        opt.skeleton = u.load_asset(existing).get_editor_property("skeleton")
    task = u.AssetImportTask()
    task.filename = model
    task.destination_path = dest
    task.destination_name = name
    task.automated = True; task.save = False; task.replace_existing = True
    task.options = opt; task.factory = u.FbxFactory()
    mesh = next((o for o in run(task) if isinstance(o, u.SkeletalMesh)), None)
    if not mesh:
        failed.append(f"{name} mesh")
        continue
    slots = mesh.get_editor_property("materials")
    for i in range(len(slots)):
        slot = slots[i]
        slot.material_interface = material
        slots[i] = slot
    mesh.set_editor_property("materials", slots)
    skeleton = mesh.get_editor_property("skeleton")
    physics = mesh.get_editor_property("physics_asset")
    if not physics:
        try:
            physics = u.SkeletalMeshEditorSubsystem.create_physics_asset(mesh, True)
        except Exception:
            physics = None
    if not physics:
        failed.append(f"{name} physics asset")
    u.EditorAssetLibrary.save_loaded_asset(mesh, False)
    u.EditorAssetLibrary.save_loaded_asset(skeleton, False)
    if physics:
        u.EditorAssetLibrary.save_loaded_asset(physics, False)

    # Its clips.
    report = json.load(open(os.path.join(clip_root, name, "prepare_report.json")))
    for clip, info in report.get(name, {}).items():
        if "skipped" in info:
            failed.append(f"{name}/{clip} ({info['skipped']})")
            continue
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
        task.filename = os.path.join(clip_root, name, name, clip + ".fbx")
        task.destination_path = dest + "/Animations"
        task.destination_name = clip
        task.automated = True; task.save = True; task.replace_existing = True
        task.options = opt; task.factory = u.FbxFactory()
        anims = [o for o in run(task) if isinstance(o, u.AnimSequence)]
        (done if len(anims) == 1 and anims[0].get_editor_property("skeleton") == skeleton else failed).append(f"{name}/{clip}")

if failed:
    u.log_error("SWAMP ENEMY IMPORT FAILED: " + ", ".join(failed))
    sys.exit(1)
u.log(f"SWAMP ENEMY IMPORT PASSED: {len(done)} clips ({', '.join(done)})")
