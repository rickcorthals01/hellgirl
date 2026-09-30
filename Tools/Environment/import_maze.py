# Unreal (run through build_maze_kit.ps1): brings in what the Frozen Maze (World IV) needs, in /Game/Environment/Maze.
#   M_MazeIce        the kit's ice: vertex colours, glossy, with a cold rim of light on the faces turned away from you.
#                    MI_MazeCrystal (the crystals' cyan glow) and MI_MazeShadowGlow (the lairs' violet edge) glow brighter.
#   M_MazeIceShell   clear, glossy, see-through ice (the block round the frozen remains).
#   M_MazeFloor      the cracked frozen floor, in world space: frost patches, faintly glowing cracks, and near the vortex
#                    a dark, clear frozen lake whose cracks glow purple.
#   M_MazeFall       falling water for the waterfalls and curtains: streaks pouring down, soft edges, foam at the foot.
#   M_MazeVortex     the vortex: glowing purple spiral arms turning round a bright core (additive).
#   MI_MazePool      the waterfalls' pools: the swamp's water, icy blue.
#   M_MazeImprint    the boss room's charred imprint of the vortex (burnt spiral, smouldering at the heart).
#   M_MazeBolt       the boss room's purple lightning.
#   Kit              the meshes from maze_kit.py in /Game/Environment/Maze/Kit. The walls have no collision of their own
#                    (the level puts simple blockers along them); spires and frozen remains collide with their triangles.
# Needs the graveyard and swamp art first (T_GraveNoise, M_SwampWater).
import glob, os, sys
import unreal as u

SOURCE = os.environ["HELLGIRL_KIT_SOURCE"]
ROOT = "/Game/Environment/Maze"
tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary
meshes_sub = u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.EditorStaticMeshLibrary
u.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
failed = []
noise_tex = u.load_asset("/Game/Environment/Graveyard/Textures/T_GraveNoise")
water = u.load_asset("/Game/Environment/Swamp/M_SwampWater")
if not noise_tex or not water:
    u.log_error("MAZE IMPORT FAILED: build the graveyard and swamp art first (T_GraveNoise, M_SwampWater)")
    sys.exit(1)
MASKS = u.MaterialSamplerType.SAMPLERTYPE_MASKS
F1, F2, F3 = u.CustomMaterialOutputType.CMOT_FLOAT1, u.CustomMaterialOutputType.CMOT_FLOAT2, u.CustomMaterialOutputType.CMOT_FLOAT3


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


def wire(material, expression, **sources):
    """Connects each named input of a custom node: source expression, or (expression, output name)."""
    for pin, source in sources.items():
        src, out = source if isinstance(source, tuple) else (source, "")
        lib.connect_material_expressions(src, out, expression, pin)
    if None in lib.get_inputs_for_material_expression(material, expression):
        failed.append(f"{material.get_name()} inputs")
    return expression


def finish(material):
    lib.recompile_material(material)
    u.EditorAssetLibrary.save_loaded_asset(material, False)


# --- Ice (the kit) --------------------------------------------------------------------------------------------------
m = fresh("M_MazeIce")
m.set_editor_property("used_with_instanced_static_meshes", True)
m.set_editor_property("used_with_skeletal_mesh", True)  # MI_DeprivedShadow dresses the Deprived's stand-in body
m.set_editor_property("two_sided", True)
vcol = node(m, u.MaterialExpressionVertexColor, -1000, -300)
look = wire(m, custom(m, -600, -300, """
return Col * Tint.rgb * lerp(1.0 - Variation, 1.0 + Variation, Rand);
""", ["Col", "Tint", "Rand", "Variation"], F3),
    Col=vcol, Tint=vector(m, "Tint", -1000, -200, (1, 1, 1, 1)), Rand=node(m, u.MaterialExpressionPerInstanceRandom, -1000, -120),
    Variation=scalar(m, "Variation", -1000, -40, .15))
lib.connect_material_property(look, "", u.MaterialProperty.MP_BASE_COLOR)
lib.connect_material_property(scalar(m, "Roughness", -600, -120, .22), "", u.MaterialProperty.MP_ROUGHNESS)
lib.connect_material_property(scalar(m, "Specular", -600, -40, .8), "", u.MaterialProperty.MP_SPECULAR)
rim = wire(m, custom(m, -600, 150, """
float f = pow(1.0 - saturate(abs(dot(normalize(N), normalize(V)))), RimPower);
return Col * (BaseGlow + Rim * f) * RimTint.rgb;
""", ["Col", "N", "V", "BaseGlow", "Rim", "RimPower", "RimTint"], F3),
    Col=look, N=node(m, u.MaterialExpressionVertexNormalWS, -1000, 100), V=node(m, u.MaterialExpressionCameraVectorWS, -1000, 170),
    BaseGlow=scalar(m, "BaseGlow", -1000, 240, .02), Rim=scalar(m, "Rim", -1000, 310, .9), RimPower=scalar(m, "RimPower", -1000, 380, 3.0),
    RimTint=vector(m, "RimTint", -1000, 450, (.6, .85, 1.0, 1)))
lib.connect_material_property(rim, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
finish(m)
ice = m
for name, glow_amount, rim_amount, rough in (("MI_MazeCrystal", 2.2, 1.0, .1), ("MI_MazeShadowGlow", 1.8, 0.0, .5)):
    mi = instance(name, ice)
    lib.set_material_instance_scalar_parameter_value(mi, "BaseGlow", glow_amount)
    lib.set_material_instance_scalar_parameter_value(mi, "Rim", rim_amount)
    lib.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    lib.set_material_instance_scalar_parameter_value(mi, "Variation", 0.0)
    lib.set_material_instance_vector_parameter_value(mi, "RimTint", u.LinearColor(1, 1, 1, 1))
    u.EditorAssetLibrary.save_loaded_asset(mi, False)
# The Deprived's stand-in (until its own model): all black, with a violet rim of light at its edges.
shade_mi = instance("MI_DeprivedShadow", ice)
for param, value in (("BaseGlow", 0.0), ("Rim", 1.6), ("RimPower", 2.5), ("Roughness", .35), ("Variation", 0.0)):
    lib.set_material_instance_scalar_parameter_value(shade_mi, param, value)
lib.set_material_instance_vector_parameter_value(shade_mi, "Tint", u.LinearColor(.012, .01, .016, 1))
lib.set_material_instance_vector_parameter_value(shade_mi, "RimTint", u.LinearColor(40, 12, 70, 1))  # (x the near-black tint)
u.EditorAssetLibrary.save_loaded_asset(shade_mi, False)

# --- Clear ice shell ------------------------------------------------------------------------------------------------
s = fresh("M_MazeIceShell")
s.set_editor_property("blend_mode", u.BlendMode.BLEND_TRANSLUCENT)
try:
    s.set_editor_property("translucency_lighting_mode", u.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
except Exception as e:
    u.log_warning(f"translucency lighting mode not set: {e}")
fres = wire(s, custom(s, -600, 100, "return pow(1.0 - saturate(abs(dot(normalize(N), normalize(V)))), 2.5);", ["N", "V"], F1),
            N=node(s, u.MaterialExpressionVertexNormalWS, -1000, 100), V=node(s, u.MaterialExpressionCameraVectorWS, -1000, 170))
color = vector(s, "Color", -1000, -250, (.25, .45, .62, 1))
lib.connect_material_property(color, "", u.MaterialProperty.MP_BASE_COLOR)
lib.connect_material_property(scalar(s, "Roughness", -600, -150, .06), "", u.MaterialProperty.MP_ROUGHNESS)
lib.connect_material_property(scalar(s, "Specular", -600, -80, 1.0), "", u.MaterialProperty.MP_SPECULAR)
opacity = wire(s, custom(s, -300, 100, "return saturate(Opacity + F * 0.5);", ["Opacity", "F"], F1),
               Opacity=scalar(s, "Opacity", -600, 250, .2), F=fres)
lib.connect_material_property(opacity, "", u.MaterialProperty.MP_OPACITY)
glow = wire(s, custom(s, -300, -20, "return Color.rgb * (0.05 + F * RimGlow);", ["Color", "F", "RimGlow"], F3),
            Color=color, F=fres, RimGlow=scalar(s, "RimGlow", -600, 330, 1.2))
lib.connect_material_property(glow, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
finish(s)

# --- Frozen floor ---------------------------------------------------------------------------------------------------
f = fresh("M_MazeFloor")
wp = node(f, u.MaterialExpressionWorldPosition, -1600, 0)
noise_samples = []
for i, (scale, offset) in enumerate(((2400.0, 0.0), (650.0, .37))):
    c = wire(f, custom(f, -1300, -500 + i * 250, f"return WP.xy / {scale} + {offset};", ["WP"], F2), WP=wp)
    t = node(f, u.MaterialExpressionTextureSample, -1000, -500 + i * 250, texture=noise_tex, sampler_type=MASKS)
    lib.connect_material_expressions(c, "", t, "UVs")
    noise_samples.append(t)
cracks = wire(f, custom(f, -1000, 100, """
float2 res = 0;
for (int s = 0; s < 2; s++)
{
    float scale = s == 0 ? Big : Fine;
    float2 p = WP.xy / scale;
    // Warped, so the plates are irregular shards rather than a tiling of cells.
    p += float2(sin(p.y * 1.3 + sin(p.x * 0.7) * 1.7), sin(p.x * 1.1 + sin(p.y * 0.9) * 1.3)) * 0.35;
    float2 ip = floor(p);
    float2 fp = p - ip;
    float d1 = 8.0, d2 = 8.0;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++)
        {
            float2 g = float2(i, j);
            float2 h = ip + g;
            float2 o = frac(sin(float2(dot(h, float2(127.1, 311.7)), dot(h, float2(269.5, 183.3)))) * 43758.5453);
            float2 r = g + o - fp;
            float d = dot(r, r);
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) { d2 = d; }
        }
    float e = sqrt(d2) - sqrt(d1);
    res[s] = 1.0 - smoothstep(0.0, Width * (s == 0 ? 1.0 : 1.5), e);
}
return res;
""", ["WP", "Big", "Fine", "Width"], F2), WP=wp, Big=scalar(f, "CrackScale", -1300, 150, 540.0), Fine=scalar(f, "FineCrackScale", -1300, 220, 150.0),
    Width=scalar(f, "CrackWidth", -1300, 290, .035))
# Where cracks show: some stretches of ice are crazed with them, others nearly clear.
vis = wire(f, custom(f, -700, 0, "return saturate(A * 1.6 - 0.15);", ["A"], F1), A=(noise_samples[0], "R"))
vortex_pos = vector(f, "VortexPosition", -1300, 400, (0, 0, 0, 1))
lake = wire(f, custom(f, -1000, 400, "return saturate((LakeRadius - distance(WP.xy, VP.xy)) / 500.0);", ["WP", "VP", "LakeRadius"], F1),
            WP=wp, VP=vortex_pos, LakeRadius=scalar(f, "LakeRadius", -1300, 470, 2400.0))
frost = wire(f, custom(f, -700, -400, "return saturate(A * 1.5 - 0.35) * 0.75 + saturate(B - 0.55) * 0.7;", ["A", "B"], F1),
             A=(noise_samples[0], "R"), B=(noise_samples[1], "R"))
base = wire(f, custom(f, -400, -300, """
float3 c = lerp(Deep.rgb, Light.rgb, Frost);
c = lerp(c, Deep.rgb * 0.5, Lake * 0.9);
return c * (1.0 - (Cracks.x * 0.55 + Cracks.y * 0.2) * Vis);
""", ["Frost", "Lake", "Cracks", "Vis", "Deep", "Light"], F3), Frost=frost, Lake=lake, Cracks=cracks, Vis=vis,
    Deep=vector(f, "Deep", -700, -250, (.012, .03, .06, 1)), Light=vector(f, "Light", -700, -180, (.2, .32, .44, 1)))
lib.connect_material_property(base, "", u.MaterialProperty.MP_BASE_COLOR)
rough = wire(f, custom(f, -400, -100, "return lerp(lerp(0.1, 0.6, Frost), 0.03, Lake);", ["Frost", "Lake"], F1), Frost=frost, Lake=lake)
lib.connect_material_property(rough, "", u.MaterialProperty.MP_ROUGHNESS)
emissive = wire(f, custom(f, -400, 150, """
float v = saturate(1.0 - distance(WP.xy, VP.xy) / Reach);
float3 e = CrackColor.rgb * (Cracks.x + Cracks.y * 0.3) * Vis * CrackGlow;
e += VortexColor.rgb * (Cracks.x + Cracks.y * 0.6) * VortexGlow * v * v;
e += VortexColor.rgb * 0.03 * Lake * VortexGlow;
return e;
""", ["WP", "VP", "Cracks", "Vis", "Lake", "CrackColor", "CrackGlow", "VortexColor", "VortexGlow", "Reach"], F3),
    WP=wp, VP=vortex_pos, Cracks=cracks, Vis=vis, Lake=lake, CrackColor=vector(f, "CrackColor", -700, 250, (.2, .55, .9, 1)),
    CrackGlow=scalar(f, "CrackGlow", -700, 320, .06), VortexColor=vector(f, "VortexColor", -700, 390, (.55, .12, 1.0, 1)),
    VortexGlow=scalar(f, "VortexGlow", -700, 460, 3.0), Reach=scalar(f, "VortexReach", -700, 530, 3600.0))
lib.connect_material_property(emissive, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
finish(f)

# --- Falling water --------------------------------------------------------------------------------------------------
# The sheet's UVs run 0..1 across (U) and from the top down (V); TileU / TileV say how many 4 m tiles it spans.
w = fresh("M_MazeFall")
w.set_editor_property("blend_mode", u.BlendMode.BLEND_TRANSLUCENT)
w.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
w.set_editor_property("two_sided", True)
uv = node(w, u.MaterialExpressionTextureCoordinate, -1600, 0)
time = node(w, u.MaterialExpressionTime, -1600, 100)
tile_u, tile_v, speed = scalar(w, "TileU", -1600, 200, 2.0), scalar(w, "TileV", -1600, 270, 6.0), scalar(w, "Speed", -1600, 340, .9)
streaks = []
for i, (su, sv, sp, off) in enumerate(((3.0, .22, 1.0, 0.0), (5.5, .35, 1.4, .3))):
    c = wire(w, custom(w, -1300, -300 + i * 250, f"return float2(UV.x * TileU * {su} + {off}, UV.y * TileV * {sv} - Time * Speed * {sp});",
                       ["UV", "TileU", "TileV", "Time", "Speed"], F2), UV=uv, TileU=tile_u, TileV=tile_v, Time=time, Speed=speed)
    t = node(w, u.MaterialExpressionTextureSample, -1000, -300 + i * 250, texture=noise_tex, sampler_type=MASKS)
    lib.connect_material_expressions(c, "", t, "UVs")
    streaks.append(t)
density = wire(w, custom(w, -700, 0, """
float streak = saturate(A * 2.2 - 0.9) * 0.7 + saturate(B * 2.4 - 1.1) * 0.9;
float edge = smoothstep(0.0, 0.1, UV.x) * smoothstep(1.0, 0.9, UV.x);
float top = smoothstep(0.0, 0.5, UV.y);  // it comes out of the dark as spray and gathers as it falls
float foam = smoothstep(0.88, 1.0, UV.y);
return saturate(((0.12 + streak) * Opacity + foam * 0.35) * edge * top);
""", ["A", "B", "UV", "Opacity"], F1), A=(streaks[0], "R"), B=(streaks[1], "R"), UV=uv, Opacity=scalar(w, "Opacity", -1000, 250, .6))
fade = node(w, u.MaterialExpressionDepthFade, -400, 100)
lib.connect_material_expressions(density, "", fade, "Opacity")
lib.connect_material_expressions(scalar(w, "FadeDistance", -700, 200, 80.0), "", fade, "FadeDistance")
lib.connect_material_property(fade, "", u.MaterialProperty.MP_OPACITY)
fall_glow = wire(w, custom(w, -400, -150, "return Color.rgb * Glow * (0.45 + D);", ["Color", "Glow", "D"], F3),
                 Color=vector(w, "Color", -700, -250, (.45, .75, 1.0, 1)), Glow=scalar(w, "Glow", -700, -180, 1.4), D=density)
lib.connect_material_property(fall_glow, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
finish(w)

# --- The vortex -----------------------------------------------------------------------------------------------------
v = fresh("M_MazeVortex")
v.set_editor_property("blend_mode", u.BlendMode.BLEND_ADDITIVE)
v.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
v.set_editor_property("two_sided", True)
vuv = node(v, u.MaterialExpressionTextureCoordinate, -1400, 0)
vtime = node(v, u.MaterialExpressionTime, -1400, 100)
vspeed = scalar(v, "Speed", -1400, 200, 1.4)
polar = wire(v, custom(v, -1100, -250, """
float2 c = UV - 0.5;
float r = length(c) * 2.0;
float a = atan2(c.y, c.x) / 6.2831853;
return float2(a * 3.0 + r * 0.8 + Time * Speed * 0.08, r * 1.3 - Time * 0.15);
""", ["UV", "Time", "Speed"], F2), UV=vuv, Time=vtime, Speed=vspeed)
wisp = node(v, u.MaterialExpressionTextureSample, -800, -250, texture=noise_tex, sampler_type=MASKS)
lib.connect_material_expressions(polar, "", wisp, "UVs")
swirl = wire(v, custom(v, -500, 0, """
float2 c = UV - 0.5;
float r = length(c) * 2.0;
float a = atan2(c.y, c.x);
float spiral = 0.5 + 0.5 * sin(a * Arms + log(r + 0.02) * Twist + Time * Speed);
float arms = pow(spiral, 3.0) * (0.5 + W) * 1.4;
float fade = saturate(1.0 - r); fade *= fade;
// A bright ring round a small core, the arms turning out from it.
float ring = exp(-pow((r - 0.28) / 0.08, 2.0));
float core = exp(-r * r * 60.0);
float3 col = lerp(Edge.rgb, Mid.rgb, saturate(1.3 - r * 1.2)) * (arms * fade + ring * 1.2) + Core.rgb * core * 0.9;
return col * Glow;
""", ["UV", "Time", "Speed", "W", "Arms", "Twist", "Edge", "Mid", "Core", "Glow"], F3),
    UV=vuv, Time=vtime, Speed=vspeed, W=(wisp, "R"), Arms=scalar(v, "Arms", -800, 100, 3.0), Twist=scalar(v, "Twist", -800, 170, 5.0),
    Edge=vector(v, "Edge", -800, 240, (.2, .02, .55, 1)), Mid=vector(v, "Mid", -800, 310, (.7, .22, 1.0, 1)),
    Core=vector(v, "Core", -800, 380, (1.0, .85, 1.0, 1)), Glow=scalar(v, "Glow", -800, 450, 2.5))
lib.connect_material_property(swirl, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
finish(v)

# --- The boss room: the vortex's charred imprint and the purple lightning -------------------------------------------
# The imprint lies on the ice (a flat plane, UVs 0..1): unlit and see-through, so the black blends the ice beneath it into
# char; three burnt spiral arms, ash grey on their crests, smouldering red, orange and yellow toward the heart.
im = fresh("M_MazeImprint")
im.set_editor_property("blend_mode", u.BlendMode.BLEND_TRANSLUCENT)
im.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
iuv = node(im, u.MaterialExpressionTextureCoordinate, -1400, 0)
itime = node(im, u.MaterialExpressionTime, -1400, 100)
inoise_uv = wire(im, custom(im, -1100, -250, "return UV * 2.5 + Time * float2(0.004, 0.003);", ["UV", "Time"], F2), UV=iuv, Time=itime)
inoise = node(im, u.MaterialExpressionTextureSample, -800, -250, texture=noise_tex, sampler_type=MASKS)
lib.connect_material_expressions(inoise_uv, "", inoise, "UVs")
shape = wire(im, custom(im, -500, 0, """
float2 c = (UV - 0.5) * 2.0;
float r = length(c);
float a = atan2(c.y, c.x);
float spiral = 0.5 + 0.5 * sin(a * 3.0 + log(r + 0.03) * 5.0);
float arm = smoothstep(0.3, 0.8, spiral);
float edge = 1.0 - smoothstep(0.65, 1.0, r + (N - 0.5) * 0.35);
float charred = saturate(edge * (0.8 + 0.2 * arm));
float heat = pow(saturate(1.0 - r * 3.0), 1.5) * arm + saturate(1.0 - r * 9.0);
float seam = smoothstep(0.42, 0.5, spiral) * (1.0 - smoothstep(0.5, 0.58, spiral)) * edge * saturate(1.0 - r * 1.3);
return float3(charred, heat, seam);
""", ["UV", "N"], F3), UV=iuv, N=(inoise, "R"))
iglow = wire(im, custom(im, -200, -150, """
float flick = 0.7 + 0.3 * sin(Time * 3.1 + N * 12.0) * sin(Time * 1.7 + N * 5.0);
float3 embers = lerp(Ember.rgb, Heart.rgb, saturate(S.y * 1.5 - 0.5)) * S.y * flick * (0.3 + N);
embers += Seam.rgb * S.z * flick * 0.35;
// Grey ash along the seams between the burnt (black) arms.
float ash = 0.02 * S.z * saturate(N * 1.6);
return embers * Glow + ash;
""", ["S", "N", "Time", "Ember", "Heart", "Seam", "Glow"], F3), S=shape, N=(inoise, "R"), Time=itime,
    Ember=vector(im, "Ember", -500, 200, (1.0, .28, .04, 1)), Heart=vector(im, "Heart", -500, 270, (1.0, .8, .28, 1)),
    Seam=vector(im, "Seam", -500, 340, (.9, .1, .03, 1)), Glow=scalar(im, "Glow", -500, 410, 1.4))
lib.connect_material_property(iglow, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
iop = wire(im, custom(im, -200, 100, "return saturate(S.x * 0.96 + S.y);", ["S"], F1), S=shape)
lib.connect_material_property(iop, "", u.MaterialProperty.MP_OPACITY)
finish(im)

# Lightning: ribbons drawn by the level along each bolt's jagged path (UV.y runs across the ribbon), a white-hot core
# in a violet glow; Glow is animated for the flash.
bo = fresh("M_MazeBolt")
bo.set_editor_property("blend_mode", u.BlendMode.BLEND_ADDITIVE)
bo.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
bo.set_editor_property("two_sided", True)
buv = node(bo, u.MaterialExpressionTextureCoordinate, -900, 0)
bolt = wire(bo, custom(bo, -500, 0, """
float d = abs(UV.y * 2.0 - 1.0);
float core = pow(saturate(1.0 - d), 6.0);
float halo = pow(saturate(1.0 - d), 1.5);
return (Core.rgb * core * 2.0 + Color.rgb * halo * 0.6) * Glow;
""", ["UV", "Core", "Color", "Glow"], F3), UV=buv, Core=vector(bo, "Core", -900, 150, (.95, .8, 1.0, 1)),
    Color=vector(bo, "Color", -900, 220, (.55, .15, 1.0, 1)), Glow=scalar(bo, "Glow", -900, 290, 4.0))
lib.connect_material_property(bolt, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
finish(bo)

# --- Pools ----------------------------------------------------------------------------------------------------------
pool = instance("MI_MazePool", water)
lib.set_material_instance_vector_parameter_value(pool, "Soul", u.LinearColor(.35, .72, 1.0, 1))
lib.set_material_instance_vector_parameter_value(pool, "Deep", u.LinearColor(.008, .025, .05, 1))
lib.set_material_instance_scalar_parameter_value(pool, "Glow", .22)
lib.set_material_instance_scalar_parameter_value(pool, "Veins", .9)
u.EditorAssetLibrary.save_loaded_asset(pool, False)

# --- Kit ------------------------------------------------------------------------------------------------------------
COMPLEX = {"SM_IceSpireA", "SM_IceSpireB", "SM_FrozenRemains"}
SLOT1 = {"SM_GlowCrystals": u.load_asset(f"{ROOT}/MI_MazeCrystal"), "SM_ShadowPool": u.load_asset(f"{ROOT}/MI_MazeShadowGlow"),
         "SM_FrozenRemains": s}
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
        slot.material_interface = ice if i == 0 else SLOT1.get(name, u.load_asset(f"{ROOT}/MI_MazeCrystal"))
        slots[i] = slot
    mesh.set_editor_property("static_materials", slots)
    meshes_sub.remove_collisions(mesh)
    body = mesh.get_editor_property("body_setup")
    if name in COMPLEX:
        if not body:
            failed.append(name + " (no body setup)")
            continue
        body.set_editor_property("collision_trace_flag", u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    elif body:
        body.set_editor_property("collision_trace_flag", u.CollisionTraceFlag.CTF_USE_DEFAULT)
    u.EditorAssetLibrary.save_loaded_asset(mesh, False)
    done.append(name)

u.log(f"MAZE KIT: {len(done)} meshes: {', '.join(done)}")
if failed:
    u.log_error("MAZE IMPORT FAILED: " + ", ".join(failed))
    sys.exit(1)
u.log("MAZE IMPORT PASSED")
