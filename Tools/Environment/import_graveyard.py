# Unreal (run through build_graveyard_kit.ps1): brings in everything the graveyard map needs.
#   Textures:  Poly Haven CC0 ground textures (sparse grass, gravel, mud) and the generated T_GraveNoise.
#   Materials: M_GraveGround (tiling ground: albedo x tint, normal, roughness, large-scale variation from the noise)
#              with instances MI_GraveGrass, MI_GravePath and MI_GraveDirt; M_GraveMist (translucent drifting mist).
#   Kit:       the meshes from graveyard_kit.py, with M_ForestKit / MI_ForestKitGlow and collision on solid pieces.
import glob, os, sys
import unreal as u

SOURCE = os.environ["HELLGIRL_KIT_SOURCE"]            # graveyard_kit.py output (FBX) and T_GraveNoise.png
TEXTURES_IN = os.environ["HELLGIRL_TEXTURE_SOURCE"]    # Desktop\Hellgirl Game\Textures Poly Haven
ROOT = "/Game/Environment/Graveyard"
tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary
meshes_sub = u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.EditorStaticMeshLibrary
u.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
failed = []


def run(task):
    tools.import_asset_tasks([task])
    return list(task.get_objects())


# --- Textures ---------------------------------------------------------------------------------------------------
def texture(path, name, kind):
    task = u.AssetImportTask()
    task.filename = path
    task.destination_path = f"{ROOT}/Textures"
    task.destination_name = name
    task.automated = True; task.save = True; task.replace_existing = True
    tex = next((o for o in run(task) if isinstance(o, u.Texture2D)), None)
    if not tex:
        failed.append(name)
        return None
    tex.set_editor_property("srgb", kind == "color")
    if kind == "normal":
        tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_NORMALMAP)
    elif kind == "mask":
        tex.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_MASKS)
    u.EditorAssetLibrary.save_loaded_asset(tex, False)
    return tex


tex = {}
for key, stem in (("Grass", "sparse_grass"), ("Gravel", "gravel_ground_01"), ("Mud", "brown_mud_02")):
    tex[key] = (texture(os.path.join(TEXTURES_IN, f"{stem}_Diffuse.jpg"), f"T_Grave{key}_D", "color"),
                texture(os.path.join(TEXTURES_IN, f"{stem}_nor_dx.jpg"), f"T_Grave{key}_N", "normal"),
                texture(os.path.join(TEXTURES_IN, f"{stem}_Rough.jpg"), f"T_Grave{key}_R", "mask"))
noise_tex = texture(os.path.join(SOURCE, "T_GraveNoise.png"), "T_GraveNoise", "mask")


# --- Materials --------------------------------------------------------------------------------------------------
def fresh(name):
    path = f"{ROOT}/{name}"
    if u.EditorAssetLibrary.does_asset_exist(path):
        material = u.load_asset(path)
        lib.delete_all_material_expressions(material)
        return material
    return tools.create_asset(name, ROOT, u.Material, u.MaterialFactoryNew())


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


# Ground: the procedural ground meshes carry world-space UVs (world / tile size), so the textures tile evenly.
g = fresh("M_GraveGround")
uv = node(g, u.MaterialExpressionTextureCoordinate, -1400, 0)
samples = {}
for i, (param, sampler, key) in enumerate((("Albedo", u.MaterialSamplerType.SAMPLERTYPE_COLOR, 0),
                                            ("Normal", u.MaterialSamplerType.SAMPLERTYPE_NORMAL, 1),
                                            ("Roughness", u.MaterialSamplerType.SAMPLERTYPE_MASKS, 2))):
    s = node(g, u.MaterialExpressionTextureSampleParameter2D, -1000, -400 + i * 300, parameter_name=param,
             texture=tex["Grass"][key], sampler_type=sampler)
    lib.connect_material_expressions(uv, "", s, "UVs")
    samples[param] = s
macro_uv = custom(g, -1200, 600, "return UV * Scale;", ["UV", "Scale"], u.CustomMaterialOutputType.CMOT_FLOAT2)
lib.connect_material_expressions(uv, "", macro_uv, "UV")
lib.connect_material_expressions(scalar(g, "MacroScale", -1400, 700, .045), "", macro_uv, "Scale")
macro = node(g, u.MaterialExpressionTextureSample, -1000, 600, texture=noise_tex, sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS)
lib.connect_material_expressions(macro_uv, "", macro, "UVs")
look = custom(g, -600, -400, """
float v = lerp(1.0 - Macro, 1.0 + Macro, Noise);
return Albedo * Tint.rgb * Brightness * v;
""", ["Albedo", "Tint", "Brightness", "Macro", "Noise"], u.CustomMaterialOutputType.CMOT_FLOAT3)
lib.connect_material_expressions(samples["Albedo"], "RGB", look, "Albedo")
lib.connect_material_expressions(vector(g, "Tint", -1000, -520, (1, 1, 1, 1)), "", look, "Tint")
lib.connect_material_expressions(scalar(g, "Brightness", -1000, -560, 1.0), "", look, "Brightness")
lib.connect_material_expressions(scalar(g, "MacroStrength", -1000, 800, .35), "", look, "Macro")
lib.connect_material_expressions(macro, "R", look, "Noise")
lib.connect_material_property(look, "", u.MaterialProperty.MP_BASE_COLOR)
lib.connect_material_property(samples["Normal"], "RGB", u.MaterialProperty.MP_NORMAL)
rough = custom(g, -600, 200, "return saturate(R * Scale + 0.08);", ["R", "Scale"], u.CustomMaterialOutputType.CMOT_FLOAT1)
lib.connect_material_expressions(samples["Roughness"], "R", rough, "R")
lib.connect_material_expressions(scalar(g, "RoughnessScale", -1000, 300, 1.0), "", rough, "Scale")
lib.connect_material_property(rough, "", u.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(g)
u.EditorAssetLibrary.save_loaded_asset(g, False)
if not (wired(g, look) and wired(g, rough) and wired(g, macro_uv)):
    failed.append("M_GraveGround inputs")

# Instances: dark blue-green grass, cold grey gravel paths, dark grave dirt.
for name, key, tint, brightness in (("MI_GraveGrass", "Grass", (.42, .62, .58, 1), .55), ("MI_GravePath", "Gravel", (.55, .6, .7, 1), .5),
                                     ("MI_GraveDirt", "Mud", (.7, .66, .62, 1), .6)):
    path = f"{ROOT}/{name}"
    mi = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(name, ROOT, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", g)
    for param, t in zip(("Albedo", "Normal", "Roughness"), tex[key]):
        lib.set_material_instance_texture_parameter_value(mi, param, t)
    lib.set_material_instance_vector_parameter_value(mi, "Tint", u.LinearColor(*tint))
    lib.set_material_instance_scalar_parameter_value(mi, "Brightness", brightness)
    u.EditorAssetLibrary.save_loaded_asset(mi, False)

# Mist: unlit, translucent. Two layers of the tiling noise drift past each other; the mist fades where it meets
# geometry (depth fade) and close to the camera, so it never fills the screen.
m = fresh("M_GraveMist")
m.set_editor_property("blend_mode", u.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", True)
muv = node(m, u.MaterialExpressionTextureCoordinate, -1400, 0)
time = node(m, u.MaterialExpressionTime, -1400, 120)
layers = []
for i, (scale, pan) in enumerate(((1.0, (.012, .004)), (.63, (-.007, .009)))):
    c = custom(m, -1100, -200 + i * 300, f"return UV * {scale} + Time * float2({pan[0]}, {pan[1]}) * Speed;", ["UV", "Time", "Speed"],
               u.CustomMaterialOutputType.CMOT_FLOAT2)
    lib.connect_material_expressions(muv, "", c, "UV")
    lib.connect_material_expressions(time, "", c, "Time")
    lib.connect_material_expressions(scalar(m, f"Speed{i}", -1400, 240 + i * 80, 1.0), "", c, "Speed")
    s = node(m, u.MaterialExpressionTextureSample, -800, -200 + i * 300, texture=noise_tex, sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS)
    lib.connect_material_expressions(c, "", s, "UVs")
    layers.append((c, s))
depth = node(m, u.MaterialExpressionPixelDepth, -800, 400)
density = custom(m, -450, 0, """
float d = saturate(A * 1.3) * saturate(B * 1.6 + 0.15);
float nearFade = saturate((Depth - NearStart) / NearRange);
return saturate(d * Opacity * nearFade);
""", ["A", "B", "Depth", "Opacity", "NearStart", "NearRange"], u.CustomMaterialOutputType.CMOT_FLOAT1)
lib.connect_material_expressions(layers[0][1], "R", density, "A")
lib.connect_material_expressions(layers[1][1], "R", density, "B")
lib.connect_material_expressions(depth, "", density, "Depth")
lib.connect_material_expressions(scalar(m, "Opacity", -800, 520, .55), "", density, "Opacity")
lib.connect_material_expressions(scalar(m, "NearStart", -800, 600, 250.), "", density, "NearStart")
lib.connect_material_expressions(scalar(m, "NearRange", -800, 680, 600.), "", density, "NearRange")
fade = node(m, u.MaterialExpressionDepthFade, -200, 100)
lib.connect_material_expressions(density, "", fade, "Opacity")
lib.connect_material_expressions(scalar(m, "FadeDistance", -450, 250, 120.), "", fade, "FadeDistance")
lib.connect_material_property(fade, "", u.MaterialProperty.MP_OPACITY)
lib.connect_material_property(vector(m, "MistColor", -450, -250, (.16, .19, .24, 1)), "", u.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(m)
u.EditorAssetLibrary.save_loaded_asset(m, False)
if not (wired(m, density) and all(wired(m, c) for c, _ in layers)):
    failed.append("M_GraveMist inputs")

# Sky: the forest's night sky (gradient, twinkling stars) with a large full moon (shaded maria, a soft halo) and
# thin clouds drifting across it, lit where they pass the moon.
k = fresh("M_GraveSky")
k.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
k.set_editor_property("two_sided", True)
sky = custom(k, -400, 0, """
float3 d = normalize(-CamVec);
float h = saturate(d.z);
float3 sky = lerp(Horizon, Zenith, pow(h, 0.5));
float3 cell = floor(d * 420.0);
float n = frac(sin(dot(cell, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float star = step(1.0 - StarDensity, n) * saturate((h - 0.04) * 4.0);
sky += star * (0.55 + 0.45 * sin(Time * (0.7 + n * 2.5) + n * 40.0)) * float3(0.75, 0.82, 1.0) * StarBrightness;
float3 md = normalize(MoonDir);
float m = dot(d, md);
float ang = acos(clamp(m, -1.0, 1.0));
float disc = 1.0 - smoothstep(MoonSize * 0.96, MoonSize, ang);
float3 right = normalize(cross(md, float3(0, 0, 1)));
float3 up = cross(right, md);
float3 off = d - md * m;
float2 q = float2(dot(off, right), dot(off, up)) / max(MoonSize, 0.0001);
float maria = exp(-dot(q - float2(-0.25, 0.3), q - float2(-0.25, 0.3)) * 9.0) * 0.45
            + exp(-dot(q - float2(0.3, 0.12), q - float2(0.3, 0.12)) * 14.0) * 0.4
            + exp(-dot(q - float2(0.05, -0.35), q - float2(0.05, -0.35)) * 11.0) * 0.35
            + exp(-dot(q - float2(-0.45, -0.15), q - float2(-0.45, -0.15)) * 20.0) * 0.3;
float limb = 0.75 + 0.25 * sqrt(saturate(1.0 - dot(q, q)));
float3 moon = MoonColor * MoonBrightness * (1.0 - saturate(maria)) * limb * disc;
float halo = pow(saturate(m), 400.0) * 0.8 + pow(saturate(m), 30.0) * 0.12 + pow(saturate(m), 5.0) * 0.035;
sky += MoonColor * halo;
float2 p = d.xy / max(d.z + 0.12, 0.05) * CloudScale + float2(Time * 0.004, Time * 0.0015);
float c = 0.0;
float amp = 0.5;
for (int i = 0; i < 5; i++)
{
    float2 ip = floor(p);
    float2 fp = frac(p);
    fp = fp * fp * (3.0 - 2.0 * fp);
    float a = frac(sin(dot(ip, float2(127.1, 311.7))) * 43758.5453);
    float b = frac(sin(dot(ip + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
    float e = frac(sin(dot(ip + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
    float g = frac(sin(dot(ip + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
    c += amp * lerp(lerp(a, b, fp.x), lerp(e, g, fp.x), fp.y);
    p *= 2.03;
    amp *= 0.5;
}
float cloud = smoothstep(CloudCover, CloudCover + 0.25, c) * saturate(h * 6.0);
float3 cloudColor = lerp(float3(0.008, 0.011, 0.02), MoonColor * 0.3, pow(saturate(m), 14.0) + 0.04);
return lerp(sky + moon, cloudColor, cloud * 0.85);
""", ["CamVec", "Time", "Zenith", "Horizon", "MoonDir", "MoonColor", "StarDensity", "StarBrightness", "MoonBrightness", "MoonSize",
      "CloudScale", "CloudCover"], u.CustomMaterialOutputType.CMOT_FLOAT3)
lib.connect_material_expressions(node(k, u.MaterialExpressionCameraVectorWS, -900, -300), "", sky, "CamVec")
lib.connect_material_expressions(node(k, u.MaterialExpressionTime, -900, -220), "", sky, "Time")
for i, (name, value) in enumerate((("Zenith", (.003, .005, .014, 1)), ("Horizon", (.025, .036, .06, 1)), ("MoonDir", (1, 0, .3, 0)), ("MoonColor", (.86, .92, 1, 1)))):
    lib.connect_material_expressions(vector(k, name, -900, -140 + i * 110, value), "", sky, name)
for i, (name, value) in enumerate((("StarDensity", .0018), ("StarBrightness", 1.4), ("MoonBrightness", 9.), ("MoonSize", .05), ("CloudScale", .9), ("CloudCover", .55))):
    lib.connect_material_expressions(scalar(k, name, -900, 320 + i * 80, value), "", sky, name)
lib.connect_material_property(sky, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(k)
u.EditorAssetLibrary.save_loaded_asset(k, False)
if not wired(k, sky):
    failed.append("M_GraveSky inputs")

# --- Kit ----------------------------------------------------------------------------------------------------------
kit = u.load_asset("/Game/Environment/Materials/M_ForestKit")
glow = u.load_asset("/Game/Environment/Materials/MI_ForestKitGlow")
# The pieces that block movement collide with their own (low-poly) triangles: convex decomposition does nothing in
# the headless commandlet. The gate, the crypt exit, the oak and the open grave get simple invisible blockers from
# the level code instead (their triangles would close the openings or fill the crown).
SOLID = {"SM_HeadstoneRound", "SM_HeadstoneRoundSmall", "SM_HeadstoneGothic", "SM_HeadstoneCross", "SM_HeadstoneCeltic",
         "SM_HeadstoneBroken", "SM_Obelisk", "SM_Sarcophagus", "SM_MausoleumGothic", "SM_MausoleumDome",
         "SM_GraveWall", "SM_GraveWallPillar", "SM_Hedge", "SM_Well", "SM_StatueMourner", "SM_StatueAngel", "SM_BrokenColumn"}
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
    mesh = next((o for o in run(task) if isinstance(o, u.StaticMesh)), None)
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

u.log(f"GRAVEYARD KIT: {len(done)} meshes")
if failed:
    u.log_error("GRAVEYARD IMPORT FAILED: " + ", ".join(failed))
    sys.exit(1)
u.log("GRAVEYARD IMPORT PASSED")
