# Unreal: (re)creates the forest materials used by the code-built forest maps.
#   M_ForestKit - lit, two-sided, for the generated forest kit (Tools/Environment/forest_kit.py): colour comes
#                 from the mesh's vertex colours, each instance of an instanced mesh is a little lighter or darker,
#                 and vertex alpha is the sway weight (SwayAmount in cm; 0 keeps it still).
#   M_ForestSky - unlit, two-sided night sky for an inverted dome: gradient, twinkling stars, moon with halo.
# Run with the editor closed:
#   powershell -ExecutionPolicy Bypass -File Tools\Materials\create_forest_materials.ps1
import unreal as u

tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary
FOLDER = "/Game/Environment/Materials"


def fresh(name):
    path = f"{FOLDER}/{name}"
    if u.EditorAssetLibrary.does_asset_exist(path):
        material = u.load_asset(path)
        lib.delete_all_material_expressions(material)
        return material
    return tools.create_asset(name, FOLDER, u.Material, u.MaterialFactoryNew())


def node(material, cls, x, y, **props):
    expression = lib.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def vector(material, name, x, y, value):
    return node(material, u.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=u.LinearColor(*value))


def scalar(material, name, x, y, value):
    return node(material, u.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def custom(material, x, y, code, inputs, output=u.CustomMaterialOutputType.CMOT_FLOAT3):
    expression = node(material, u.MaterialExpressionCustom, x, y, code=code, output_type=output)
    pins = []
    for name in inputs:
        pin = u.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    expression.set_editor_property("inputs", pins)
    return expression


# --- Forest kit ---------------------------------------------------------------------------------------
m = fresh("M_ForestKit")
m.set_editor_property("used_with_instanced_static_meshes", True)
m.set_editor_property("two_sided", True)
vcol = node(m, u.MaterialExpressionVertexColor, -900, -300)
random = node(m, u.MaterialExpressionPerInstanceRandom, -900, -40)
look = custom(m, -560, -300, """
float3 c = Col * Tint.rgb * lerp(1.0 - Variation, 1.0 + Variation, Rand);
return c;
""", ["Col", "Tint", "Rand", "Variation"])
lib.connect_material_expressions(vcol, "", look, "Col")  # the unnamed first output is RGB
lib.connect_material_expressions(vector(m, "Tint", -900, -200, (1, 1, 1, 1)), "", look, "Tint")
lib.connect_material_expressions(random, "", look, "Rand")
lib.connect_material_expressions(scalar(m, "Variation", -900, -120, .18), "", look, "Variation")
lib.connect_material_property(look, "", u.MaterialProperty.MP_BASE_COLOR)
rough = scalar(m, "Roughness", -560, -120, .88)
lib.connect_material_property(rough, "", u.MaterialProperty.MP_ROUGHNESS)
glow = scalar(m, "Glow", -560, -20, 0.)
emissive = node(m, u.MaterialExpressionMultiply, -300, -60)
lib.connect_material_expressions(look, "", emissive, "A")
lib.connect_material_expressions(glow, "", emissive, "B")
lib.connect_material_property(emissive, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
sway = custom(m, -420, 260, """
float p = Rand * 6.2831 + WorldPos.x * 0.004 + WorldPos.y * 0.003;
return float3(sin(Time * Speed + p), cos(Time * Speed * 0.83 + p * 1.3), 0.0) * Amount * Weight;
""", ["Time", "WorldPos", "Weight", "Rand", "Amount", "Speed"])
lib.connect_material_expressions(node(m, u.MaterialExpressionTime, -900, 160), "", sway, "Time")
lib.connect_material_expressions(node(m, u.MaterialExpressionWorldPosition, -900, 240), "", sway, "WorldPos")
lib.connect_material_expressions(vcol, "A", sway, "Weight")
lib.connect_material_expressions(random, "", sway, "Rand")
lib.connect_material_expressions(scalar(m, "SwayAmount", -900, 400, 3.), "", sway, "Amount")
lib.connect_material_expressions(scalar(m, "SwaySpeed", -900, 480, 1.2), "", sway, "Speed")
lib.connect_material_property(sway, "", u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
lib.recompile_material(m)
u.EditorAssetLibrary.save_loaded_asset(m, False)
# A connection with a wrong pin name fails silently; make sure every custom-node input is wired.
wired = [e.get_name() if e else None for e in lib.get_inputs_for_material_expression(m, look)] + \
        [e.get_name() if e else None for e in lib.get_inputs_for_material_expression(m, sway)]
u.log(f"KIT MATERIAL INPUTS {wired}")
if None in wired:
    u.log_error("FOREST MATERIALS FAILED: an input of M_ForestKit is not connected")

# --- Night sky ----------------------------------------------------------------------------------------
s = fresh("M_ForestSky")
s.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
s.set_editor_property("two_sided", True)
sky = custom(s, -400, 0, """
float3 d = normalize(-CamVec);
float h = saturate(d.z);
float3 sky = lerp(Horizon, Zenith, pow(h, 0.55));
float3 cell = floor(d * 420.0);
float n = frac(sin(dot(cell, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float star = step(1.0 - StarDensity, n) * saturate((h - 0.05) * 4.0);
float twinkle = 0.55 + 0.45 * sin(Time * (0.7 + n * 2.5) + n * 40.0);
sky += star * twinkle * float3(0.75, 0.82, 1.0) * StarBrightness;
float m = dot(d, normalize(MoonDir));
float disc = smoothstep(0.99925, 0.99955, m);
float halo = pow(saturate(m), 250.0) * 0.5 + pow(saturate(m), 14.0) * 0.06;
sky += MoonColor * (disc * MoonBrightness + halo);
return sky;
""", ["CamVec", "Time", "Zenith", "Horizon", "MoonDir", "MoonColor", "StarDensity", "StarBrightness", "MoonBrightness"])
lib.connect_material_expressions(node(s, u.MaterialExpressionCameraVectorWS, -900, -300), "", sky, "CamVec")
lib.connect_material_expressions(node(s, u.MaterialExpressionTime, -900, -220), "", sky, "Time")
lib.connect_material_expressions(vector(s, "Zenith", -900, -140, (.004, .007, .018, 1)), "", sky, "Zenith")
lib.connect_material_expressions(vector(s, "Horizon", -900, -20, (.03, .062, .066, 1)), "", sky, "Horizon")
lib.connect_material_expressions(vector(s, "MoonDir", -900, 100, (.8, .3, .5, 0)), "", sky, "MoonDir")
lib.connect_material_expressions(vector(s, "MoonColor", -900, 220, (.8, .88, 1., 1)), "", sky, "MoonColor")
lib.connect_material_expressions(scalar(s, "StarDensity", -900, 340, .0016), "", sky, "StarDensity")
lib.connect_material_expressions(scalar(s, "StarBrightness", -900, 420, 1.3), "", sky, "StarBrightness")
lib.connect_material_expressions(scalar(s, "MoonBrightness", -900, 500, 5.), "", sky, "MoonBrightness")
lib.connect_material_property(sky, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(s)
u.EditorAssetLibrary.save_loaded_asset(s, False)

ok = all(u.EditorAssetLibrary.does_asset_exist(f"{FOLDER}/{n}") for n in ("M_ForestKit", "M_ForestSky"))
u.log("FOREST MATERIALS " + ("PASSED" if ok else "FAILED"))
