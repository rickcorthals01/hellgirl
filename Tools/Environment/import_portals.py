# Unreal (run through import_portals.ps1): the portal art (2026-09-27, the new art style).
#   Textures: T_PortalBlue and T_PortalPurple, from the painted PNGs ("BluePortal PNG.png", "PurplePortal PNG.png").
#   Material: M_Portal, unlit and translucent. It shows the painting (its alpha cuts out the oval) and runs a second,
#             brighter copy of it upward inside the oval, so the streaks flow. Parameters: Portal (the texture),
#             Strength (overall glow), Flow (how bright the moving streaks are), Fade (0-1, for opening and closing).
# Used by Levels/WavePortal.cpp on a camera-facing card.
import os, sys
import unreal as u

SOURCE = os.environ["HELLGIRL_PORTAL_SOURCE"]
ROOT = "/Game/Environment/Portals"
tools = u.AssetToolsHelpers.get_asset_tools()
lib = u.MaterialEditingLibrary
failed = []

textures = {}
for name, file in (("T_PortalBlue", "BluePortal PNG.png"), ("T_PortalPurple", "PurplePortal PNG.png")):
    path = os.path.join(SOURCE, file)
    if not os.path.exists(path):
        failed.append(file + " missing")
        continue
    task = u.AssetImportTask()
    task.filename = path
    task.destination_path = ROOT
    task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False
    tools.import_asset_tasks([task])
    texture = u.load_asset(f"{ROOT}/{name}")
    if not texture:
        failed.append(name)
        continue
    # A painted sprite: no mips blur across the cut-out edge at a distance, and no tiling.
    texture.set_editor_property("address_x", u.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", u.TextureAddress.TA_WRAP)
    texture.set_editor_property("lod_group", u.TextureGroup.TEXTUREGROUP_EFFECTS)
    u.EditorAssetLibrary.save_loaded_asset(texture, False)
    textures[name] = texture

path = f"{ROOT}/M_Portal"
if u.EditorAssetLibrary.does_asset_exist(path):
    m = u.load_asset(path)
    lib.delete_all_material_expressions(m)
else:
    m = tools.create_asset("M_Portal", ROOT, u.Material, u.MaterialFactoryNew())
m.set_editor_property("blend_mode", u.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", True)


def node(cls, x, y, **props):
    expression = lib.create_material_expression(m, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def scalar(name, x, y, value):
    return node(u.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


uv = node(u.MaterialExpressionTextureCoordinate, -1200, 0)
time = node(u.MaterialExpressionTime, -1200, 150)
flow_uv = node(u.MaterialExpressionCustom, -950, 150, output_type=u.CustomMaterialOutputType.CMOT_FLOAT2,
               code="return float2(UV.x, UV.y + frac(Time * 0.12));")
pins = []
for pin_name in ("UV", "Time"):
    pin = u.CustomInput(); pin.set_editor_property("input_name", pin_name); pins.append(pin)
flow_uv.set_editor_property("inputs", pins)
lib.connect_material_expressions(uv, "", flow_uv, "UV")
lib.connect_material_expressions(time, "", flow_uv, "Time")
first = textures.get("T_PortalBlue")
base = node(u.MaterialExpressionTextureSampleParameter2D, -700, -100, parameter_name="Portal", texture=first)
flow = node(u.MaterialExpressionTextureSampleParameter2D, -700, 200, parameter_name="Portal", texture=first)
lib.connect_material_expressions(uv, "", base, "UVs")
lib.connect_material_expressions(flow_uv, "", flow, "UVs")
glow = node(u.MaterialExpressionCustom, -350, 0, output_type=u.CustomMaterialOutputType.CMOT_FLOAT3,
            code="return (Base * Strength + Flowing * Flow * BaseA) * Fade;")
pins = []
for pin_name in ("Base", "BaseA", "Flowing", "Strength", "Flow", "Fade"):
    pin = u.CustomInput(); pin.set_editor_property("input_name", pin_name); pins.append(pin)
glow.set_editor_property("inputs", pins)
strength, flow_amount, fade = scalar("Strength", -700, 500, 2.2), scalar("Flow", -700, 580, .7), scalar("Fade", -700, 660, 1.0)
lib.connect_material_expressions(base, "RGB", glow, "Base")
lib.connect_material_expressions(base, "A", glow, "BaseA")
lib.connect_material_expressions(flow, "RGB", glow, "Flowing")
lib.connect_material_expressions(strength, "", glow, "Strength")
lib.connect_material_expressions(flow_amount, "", glow, "Flow")
lib.connect_material_expressions(fade, "", glow, "Fade")
opacity = node(u.MaterialExpressionMultiply, -350, 300)
lib.connect_material_expressions(base, "A", opacity, "A")
lib.connect_material_expressions(fade, "", opacity, "B")
lib.connect_material_property(glow, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
lib.connect_material_property(opacity, "", u.MaterialProperty.MP_OPACITY)
lib.recompile_material(m)
u.EditorAssetLibrary.save_loaded_asset(m, False)
if None in lib.get_inputs_for_material_expression(m, glow):
    failed.append("M_Portal inputs")

if failed:
    u.log_error("PORTAL IMPORT FAILED: " + ", ".join(failed))
else:
    u.log("PORTAL IMPORT PASSED: T_PortalBlue, T_PortalPurple, M_Portal")
