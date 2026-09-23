# Unreal (run through build.ps1): imports the FBX files written by prepare_clips.py onto each
# outfit's own skeleton as /Game/Hellgirl/Outfits/<Outfit>/Animations/<Clip>.
import json, os, sys
import unreal as u

out_root = os.environ["HELLGIRL_CLIP_DIR"]
report = json.load(open(os.path.join(out_root, "prepare_report.json")))
tools = u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
imported, failed = 0, []
for outfit, clips in report.items():
    dest = f"/Game/Hellgirl/Outfits/{outfit}"
    skeleton = u.load_asset(f"{dest}/{outfit}").get_editor_property("skeleton")
    for clip, info in clips.items():
        if "skipped" in info:
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
        task.filename = os.path.join(out_root, outfit, clip + ".fbx")
        task.destination_path = dest + "/Animations"
        task.destination_name = clip
        task.automated = True
        task.save = True
        task.replace_existing = True
        task.options = opt
        task.factory = u.FbxFactory()
        tools.import_asset_tasks([task])
        anims = [o for o in task.get_objects() if isinstance(o, u.AnimSequence)]
        if len(anims) == 1 and anims[0].get_editor_property("skeleton") == skeleton:
            imported += 1
        else:
            failed.append(f"{outfit}/{clip}")
    # Each task saves its own clip; re-saving the whole folder would rewrite unchanged assets.
if failed:
    u.log_error("CLIP IMPORT FAILED: " + ", ".join(failed))
    sys.exit(1)
u.log(f"CLIP IMPORT PASSED: {imported} clips")
