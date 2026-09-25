# Unreal: imports the prepared talkbox portraits (prepare_portraits.ps1 output) as UI textures:
#   Processed/<Set>/<Mood>.png  ->  /Game/Dialogue/Portraits/<Set>/T_<Set>_<Mood>
# Run through import_portraits.ps1.
import glob, os, sys
import unreal as u

SOURCE = os.environ["HELLGIRL_PORTRAITS"]
tools = u.AssetToolsHelpers.get_asset_tools()
done, failed = [], []
for path in sorted(glob.glob(os.path.join(SOURCE, "*", "*.png"))):
    portrait_set = os.path.basename(os.path.dirname(path))
    mood = os.path.splitext(os.path.basename(path))[0]
    task = u.AssetImportTask()
    task.filename = path
    task.destination_path = f"/Game/Dialogue/Portraits/{portrait_set}"
    task.destination_name = f"T_{portrait_set}_{mood}"
    task.automated = True
    task.replace_existing = True
    task.save = False
    tools.import_asset_tasks([task])
    texture = next((o for o in task.get_objects() if isinstance(o, u.Texture2D)), None)
    if not texture:
        failed.append(f"{portrait_set}/{mood}")
        continue
    texture.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", u.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    u.EditorAssetLibrary.save_loaded_asset(texture, False)
    done.append(f"{portrait_set}/{mood}")
u.log("PORTRAITS IMPORTED: " + ", ".join(done))
if failed:
    u.log_error("PORTRAIT IMPORT FAILED: " + ", ".join(failed))
    sys.exit(1)
u.log("PORTRAIT IMPORT PASSED")
