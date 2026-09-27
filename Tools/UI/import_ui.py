# Unreal: imports the prepared placeholder menu art (prepare_ui.ps1 output) as UI textures:
#   UI/Processed/<Name>.png  ->  /Game/UI/Placeholder/T_<Name>
# Run through import_ui.ps1.
import glob, os, sys
import unreal as u

SOURCE = os.environ["HELLGIRL_UI"]
tools = u.AssetToolsHelpers.get_asset_tools()
done, failed = [], []
for path in sorted(glob.glob(os.path.join(SOURCE, "*.png"))):
    name = os.path.splitext(os.path.basename(path))[0]
    task = u.AssetImportTask()
    task.filename = path
    task.destination_path = "/Game/UI/Placeholder"
    task.destination_name = f"T_{name}"
    task.automated = True
    task.replace_existing = True
    task.save = False
    tools.import_asset_tasks([task])
    texture = next((o for o in task.get_objects() if isinstance(o, u.Texture2D)), None)
    if not texture:
        failed.append(name)
        continue
    texture.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", u.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    u.EditorAssetLibrary.save_loaded_asset(texture, False)
    done.append(name)
u.log("UI IMPORTED: " + ", ".join(done))
if failed:
    u.log_error("UI IMPORT FAILED: " + ", ".join(failed))
    sys.exit(1)
u.log("UI IMPORT PASSED")
