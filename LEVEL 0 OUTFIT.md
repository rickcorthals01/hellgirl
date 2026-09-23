# Hellgirl level-0 appearance

Active model: /Game/HellgirlTest/Level0/Hellgirl_Level0. The player constructor now loads this model. It uses the original skin, face and eye materials with correctly saved material-slot assignments, black masked hair materials, a worn brown cloth top, a fitted lower underwrap and a ragged outer hip wrap.

Editable source: Developer idea folder lol/Stage 01 Hands/Assets/Player Character - Hellgirl/Model/Purchased_Base/Level0/Hellgirl_Level0.blend. The original long-hair model is retained. The outfit shares the existing skeleton and retains 51 facial morph targets. Cloth inherits the body skin weights; there is no cloth simulation. This is a first outfit fit, with pose and clipping polish still appropriate during interactive combat testing.

Build and all three unattended map encounter checks passed (Level0BuildLog.txt and Level0Runtime.log). Blender previews are saved alongside the source. Import details are in Unreal_checks.json. Scripts: Animation Testing/build_lvl0.py, import_lvl0.py, pose_lvl0.py.

Latest revision: worn-white fabric, original fitted top retained with upper-arm/underarm cloth removed, and outer hip-wrap length reduced to 56% of the first version. Underwrap shortened to suit. Black hair has softer card geometry, uneven lower layers and fine side/back/fringe strands. This is a styled mesh, not simulated loose-hair physics.

Current editable source: Level0/Hellgirl_Level0_WhiteRags.blend. Current export: Hellgirl_WhiteRags.fbx. Current previews: Hellgirl_WhiteRags_preview.png and Hellgirl_WildHair_preview.png. The previous Hellgirl_Level0.blend is preserved. Rebuild this revision with Animation Testing/refine_lvl0.py, then import_refined_lvl0.py.
