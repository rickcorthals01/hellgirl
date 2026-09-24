# Unreal: imports the generated forest kit (forest_kit.py output) into /Game/Environment/ForestKit, gives each
# mesh M_ForestKit (slot 0) and its glowing variant MI_ForestKitGlow (slot 1), and adds collision to the pieces
# you can bump into. Run through build_forest_kit.ps1.
import glob, os, sys
import unreal as u

SOURCE = os.environ["HELLGIRL_KIT_SOURCE"]
DEST = "/Game/Environment/ForestKit"
MATERIALS = "/Game/Environment/Materials"
# Pieces that block movement get convex collision; everything else is scenery.
SOLID = {"SM_RockA", "SM_RockB", "SM_RockC", "SM_StandingStone", "SM_Log", "SM_Stump", "SM_Gateway", "SM_Campfire"}
tools = u.AssetToolsHelpers.get_asset_tools()
# The editor subsystem is not available in the headless commandlet; the older library has the same calls.
meshes_sub = u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.EditorStaticMeshLibrary
u.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")

kit = u.load_asset(f"{MATERIALS}/M_ForestKit")
glow_path = f"{MATERIALS}/MI_ForestKitGlow"
glow = u.load_asset(glow_path) if u.EditorAssetLibrary.does_asset_exist(glow_path) else \
    tools.create_asset("MI_ForestKitGlow", MATERIALS, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
glow.set_editor_property("parent", kit)
u.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(glow, "Glow", 7.0)
u.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(glow, "Variation", 0.0)
u.EditorAssetLibrary.save_loaded_asset(glow, False)

failed, done = [], []
for path in sorted(glob.glob(os.path.join(SOURCE, "SM_*.fbx"))):
    name = os.path.splitext(os.path.basename(path))[0]
    opt = u.FbxImportUI()
    opt.automated_import_should_detect_type = False
    opt.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    opt.import_mesh = True
    opt.import_materials = False
    opt.import_textures = False
    opt.import_as_skeletal = False
    data = opt.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("vertex_color_import_option", u.VertexColorImportOption.REPLACE)
    data.set_editor_property("normal_import_method", u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = u.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = opt
    task.factory = u.FbxFactory()
    tools.import_asset_tasks([task])
    mesh = next((o for o in task.get_objects() if isinstance(o, u.StaticMesh)), None)
    if not mesh:
        failed.append(name)
        continue
    slots = mesh.get_editor_property("static_materials")
    for i in range(len(slots)):
        slot = slots[i]
        slot.material_interface = kit if i == 0 else glow
        slots[i] = slot
    mesh.set_editor_property("static_materials", slots)
    meshes_sub.remove_collisions(mesh)
    if name in SOLID:
        meshes_sub.set_convex_decomposition_collisions(mesh, 6 if name in ("SM_Gateway", "SM_Campfire") else 3, 16, 200000)
    u.EditorAssetLibrary.save_loaded_asset(mesh, False)
    done.append(f"{name}({len(slots)} slots)")

u.log("KIT IMPORTED: " + ", ".join(done))
if failed:
    u.log_error("FOREST KIT IMPORT FAILED: " + ", ".join(failed))
    sys.exit(1)
u.log("FOREST KIT IMPORT PASSED")

# Facts about the downloaded packs that the level code needs.
lib = u.MaterialEditingLibrary
for path in ("/Game/Pack_Bonus/Materials/M_Pack_Bonus_Grass_1", "/Game/Pack_Bonus/Materials/M_Pack_Bonus_Grass_2",
             "/Game/Pack_Bonus/Materials/M_Pack_Bonus_Stone_1"):
    m = u.load_asset(path)
    u.log(f"PROBE {path}: scalars {lib.get_scalar_parameter_names(m)} vectors {lib.get_vector_parameter_names(m)} class {m.get_class().get_name()}")
for i in range(1, 8):
    mesh = u.load_asset(f"/Game/Rock_Collection_04/Meshes/Rock_0{i}/StaticMeshes/SM_Rock_0{i}")
    body = mesh.get_editor_property("body_setup")
    geom = body.get_editor_property("agg_geom") if body else None
    counts = [len(geom.get_editor_property(k)) for k in ("box_elems", "sphyl_elems", "convex_elems", "sphere_elems")] if geom else None
    u.log(f"PROBE Rock_0{i}: collision {counts} trace {body.get_editor_property('collision_trace_flag') if body else None} "
          f"nanite {mesh.get_editor_property('nanite_settings').get_editor_property('enabled')}")
for path in ("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Environment/P_Fireflies", "/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Sparks/P_Embers_A",
             "/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_1", "/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_3",
             "/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Smoke/P_Smoke_Ground" if False else "/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Fire/P_Fire_Small"):
    a = u.load_asset(path)
    u.log(f"PROBE {path}: {a.get_class().get_name() if a else 'missing'}")
