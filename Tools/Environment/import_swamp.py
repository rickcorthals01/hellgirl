# Unreal (run through build_swamp_kit.ps1): brings in what the swamp map (World II) needs.
#   Materials: M_SwampWater (glowing soul water with drifting light veins), MI_SwampSoul (the zombie arms' glow),
#              M_SwampRipple (fading rings and droplets for splashes), MI_SwampGround (muddy grass, on M_GraveGround).
#   Kit:       the meshes from swamp_kit.py in /Game/Environment/Swamp/Kit, with collision on what you can stand on.
# Needs the graveyard art first (T_GraveNoise, the ground textures and M_GraveGround).
import glob, os, sys
import unreal as u

SOURCE = os.environ["HELLGIRL_KIT_SOURCE"]
ROOT = "/Game/Environment/Swamp"
GRAVE = "/Game/Environment/Graveyard"
tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary
meshes_sub = u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.EditorStaticMeshLibrary
u.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
failed = []
noise_tex = u.load_asset(f"{GRAVE}/Textures/T_GraveNoise")
if not noise_tex:
    u.log_error("SWAMP IMPORT FAILED: build the graveyard art first (T_GraveNoise)")
    sys.exit(1)


def fresh(name):
    path = f"{ROOT}/{name}"
    if u.EditorAssetLibrary.does_asset_exist(path):
        material = u.load_asset(path)
        lib.delete_all_material_expressions(material)
        return material
    return tools.create_asset(name, ROOT, u.Material, u.MaterialFactoryNew())


def instance(name, parent):
    path = f"{ROOT}/{name}"
    mi = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(name, ROOT, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    return mi


def node(material, cls, x, y, **props):
    expression = lib.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def scalar(material, name, x, y, value):
    return node(material, u.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def vector(material, name, x, y, value):
    return node(material, u.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=u.LinearColor(*value))


def custom(material, x, y, code, inputs, output):
    expression = node(material, u.MaterialExpressionCustom, x, y, code=code, output_type=output)
    pins = []
    for name in inputs:
        pin = u.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    expression.set_editor_property("inputs", pins)
    return expression


def wired(material, expression):
    return None not in lib.get_inputs_for_material_expression(material, expression)


# --- Soul water ---------------------------------------------------------------------------------------------------
# Opaque and glossy: a deep blue base, glowing light-blue, with bright veins where two drifting noise layers cross
# (like light playing on the water). The surface sits over an invisible floor, so feet wade just below it.
w = fresh("M_SwampWater")
uv = node(w, u.MaterialExpressionTextureCoordinate, -1400, 0)
time = node(w, u.MaterialExpressionTime, -1400, 120)
samples = []
for i, (scale, pan) in enumerate(((1.0, (.018, .007)), (.71, (-.011, .015)))):
    c = custom(w, -1100, -200 + i * 300, f"return UV * {scale} + Time * float2({pan[0]}, {pan[1]});", ["UV", "Time"], u.CustomMaterialOutputType.CMOT_FLOAT2)
    lib.connect_material_expressions(uv, "", c, "UV")
    lib.connect_material_expressions(time, "", c, "Time")
    s = node(w, u.MaterialExpressionTextureSample, -800, -200 + i * 300, texture=noise_tex, sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS)
    lib.connect_material_expressions(c, "", s, "UVs")
    samples.append((c, s))
glow = custom(w, -450, -100, """
float veins = pow(saturate(1.0 - abs(A - B) * 3.0), 7.0);
float body = 0.55 + 0.45 * saturate(A + B);
return Soul.rgb * (Glow * body + Veins * veins);
""", ["A", "B", "Soul", "Glow", "Veins"], u.CustomMaterialOutputType.CMOT_FLOAT3)
lib.connect_material_expressions(samples[0][1], "R", glow, "A")
lib.connect_material_expressions(samples[1][1], "R", glow, "B")
lib.connect_material_expressions(vector(w, "Soul", -800, 400, (.2, .55, 1.0, 1)), "", glow, "Soul")
lib.connect_material_expressions(scalar(w, "Glow", -800, 480, .32), "", glow, "Glow")
lib.connect_material_expressions(scalar(w, "Veins", -800, 560, 1.3), "", glow, "Veins")
lib.connect_material_property(glow, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
lib.connect_material_property(vector(w, "Deep", -450, 200, (.01, .035, .06, 1)), "", u.MaterialProperty.MP_BASE_COLOR)
lib.connect_material_property(scalar(w, "Roughness", -450, 300, .12), "", u.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(w)
u.EditorAssetLibrary.save_loaded_asset(w, False)
if not (wired(w, glow) and all(wired(w, c) for c, _ in samples)):
    failed.append("M_SwampWater inputs")

# --- Ripples and droplets -------------------------------------------------------------------------------------------
r = fresh("M_SwampRipple")
r.set_editor_property("blend_mode", u.BlendMode.BLEND_TRANSLUCENT)
r.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
r.set_editor_property("two_sided", True)
lib.connect_material_property(vector(r, "Color", -500, 0, (.55, .88, 1.0, 1)), "", u.MaterialProperty.MP_EMISSIVE_COLOR)
lib.connect_material_property(scalar(r, "Opacity", -500, 150, .8), "", u.MaterialProperty.MP_OPACITY)
lib.recompile_material(r)
u.EditorAssetLibrary.save_loaded_asset(r, False)

# --- The arms' soul glow and the muddy ground ----------------------------------------------------------------------
kit = u.load_asset("/Game/Environment/Materials/M_ForestKit")
glow_mi = u.load_asset("/Game/Environment/Materials/MI_ForestKitGlow")
soul = instance("MI_SwampSoul", kit)
lib.set_material_instance_scalar_parameter_value(soul, "Glow", 1.3)
lib.set_material_instance_scalar_parameter_value(soul, "Variation", 0.0)
lib.set_material_instance_scalar_parameter_value(soul, "SwaySpeed", 3.0)
u.EditorAssetLibrary.save_loaded_asset(soul, False)
ground = instance("MI_SwampGround", u.load_asset(f"{GRAVE}/M_GraveGround"))
for param, tex in (("Albedo", "T_GraveGrass_D"), ("Normal", "T_GraveGrass_N"), ("Roughness", "T_GraveGrass_R")):
    lib.set_material_instance_texture_parameter_value(ground, param, u.load_asset(f"{GRAVE}/Textures/{tex}"))
lib.set_material_instance_vector_parameter_value(ground, "Tint", u.LinearColor(.62, .55, .42, 1))
lib.set_material_instance_scalar_parameter_value(ground, "Brightness", .6)
lib.set_material_instance_scalar_parameter_value(ground, "MacroStrength", .45)
u.EditorAssetLibrary.save_loaded_asset(ground, False)

# --- Kit ------------------------------------------------------------------------------------------------------------
# Pieces you can stand on collide with their own triangles. The trees get simple trunk blockers from the level code
# (their branches and moss would catch the camera).
SOLID = {"SM_Boardwalk", "SM_BoardwalkBroken", "SM_MudIsland", "SM_DrownedStump", "SM_GiantLilyPad"}
SLOT1 = {"SM_ZombieArm": soul, "SM_Ripple": r}
done = []
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
    task.destination_path = f"{ROOT}/Kit"
    task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False
    task.options = opt; task.factory = u.FbxFactory()
    tools.import_asset_tasks([task])
    mesh = next((o for o in task.get_objects() if isinstance(o, u.StaticMesh)), None)
    if not mesh:
        failed.append(name)
        continue
    slots = mesh.get_editor_property("static_materials")
    for i in range(len(slots)):
        slot = slots[i]
        slot.material_interface = kit if i == 0 else SLOT1.get(name, glow_mi)
        slots[i] = slot
    mesh.set_editor_property("static_materials", slots)
    meshes_sub.remove_collisions(mesh)
    body = mesh.get_editor_property("body_setup")
    if name in SOLID:
        if not body:
            failed.append(name + " (no body setup)")
            continue
        body.set_editor_property("collision_trace_flag", u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    elif body:
        body.set_editor_property("collision_trace_flag", u.CollisionTraceFlag.CTF_USE_DEFAULT)
    u.EditorAssetLibrary.save_loaded_asset(mesh, False)
    done.append(name)

u.log(f"SWAMP KIT: {len(done)} meshes")
if failed:
    u.log_error("SWAMP IMPORT FAILED: " + ", ".join(failed))
    sys.exit(1)
u.log("SWAMP IMPORT PASSED")
