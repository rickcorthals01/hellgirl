# Environment art

The maps are still built in code, but they are now dressed with real meshes, materials and effects instead of engine cubes, spheres and cones.

## Sources

| Source | What it gives | Used in |
|---|---|---|
| **Forest kit** (generated, `Content/Environment/ForestKit`) | Pines Ã—3, dead tree, rocks, standing stone, log, stump, fern, grass Ã—2, mushrooms (plain and glowing), bramble, goblin burrow, hell rift, rune gateway, campfire | Forest run, camp, enemy portals everywhere |
| Rock_Collection_04 (Fab) | 7 realistic boulders | Forest cover, stepping stones, rocks among the trees |
| Pack_Bonus (Fab) | Tiling ground materials | Grass_1 forest floor, Stone_3 flagstones at the campfire, Stone_2 cracked floor in the Imp arena |
| Inferno_World_Free (Fab) | Hell props | Imp arena: braziers, columns, giant ribs, spikes, bones, gold, knight statues. Camp: merchant's chest, crates, vase, gold, road braziers |
| Stylish_Fire_VFX (Fab) | Niagara fire | Campfire, braziers, burning hell rifts |
| Realistic_Starter_VFX_Pack_Vol2 (Fab) | Cascade effects | Fireflies, embers, the smoke puff when an enemy arrives |

The Fab packs are downloaded into `Content/` from your Fab library (Unreal: Window â†’ Fab â†’ My Library â†’ Add to Project).

## The forest kit

The kit is generated in Blender by `Tools/Environment/forest_kit.py`. It is low-poly and faceted, with its colours stored in the vertex colours; vertex alpha controls how much each part sways. Rebuild everything with the editor closed:

```
powershell -ExecutionPolicy Bypass -File Tools\Environment\build_forest_kit.ps1
```

That (re)creates the materials (`Tools/Materials/create_forest_materials.py`), regenerates the meshes and imports them with collision on the solid pieces. The materials are:
- **M_ForestKit:** vertex colours, a slight per-instance brightness variation, and sway (`SwayAmount` in cm).
- **MI_ForestKitGlow:** the glowing parts, i.e. runes, berries, embers and glowing mushroom caps.
- **M_ForestSky:** a night-sky dome with a gradient, twinkling stars and a moon with a halo.

## Where it is built

- `Levels/ForestArt.h` / `ForestScenery.cpp`:
  - Shared helpers: `Kit`, `Boulder`, `Inferno`, `Batch` (instanced scenery), `Solid`/`Stand` (single props), `Ground`, `Disc`, `NightSky`, `Moonlight`, `Fire`, `Embers`, `Fireflies`, `PointGlow`.
  - The forest run's scenery and the camp's dressing.
- `Levels/ForestRun.cpp`: the forest room's gameplay pieces (cover, thorns, burrows, gateway).
- `Levels/ForestHub.cpp`: the camp (campfire, road braziers, merchant stall, gateway).
- `Levels/ImpArenaScenery.cpp`: World II's Inferno dressing, on top of the cavern in `MapVisuals.cpp`.
- `Enemies/EnemySpawnPoint.cpp`: every enemy spawn is a hell rift that burns while its wave comes through, or a goblin burrow in the forest (`UseBurrow`). Enemies arrive with a puff of smoke, and the old floating labels and debug circles are gone.

## Hit feedback and HUD

Every landed hit (`AArenaFighter::PlayImpact`) gives:
- **No particles:** the user found sparks too busy, so hits use only the flash, hit-stop and shake.
- **Flash:** a 0.07 s flash of light, warm for hits and cold blue for blocks.
- **Hit-stop:** a brief freeze, 0.04 s for a light hit and 0.085 s for a heavy one.
- **Camera shake:** when Hellgirl is hit, or lands a heavy blow.

Hit-stop and the shake are switched off when the command line contains "Check", because the automated checks measure timing.

The HUD (`UI/ArenaHUD.cpp`) is drawn without big boxes:
- **Top left:** title and objective.
- **Top right:** a small minimap with coins underneath.
- **Top centre:** the boss bar.
- **Bottom left:** health, energy tubes, stamina, and CHARGE, SLAM and weapon labels.
- **Centre:** move callouts that fade.
- **Controls:** shown only for the first 12 seconds of a level.

`-HellgirlMapShot -MapShotImpacts` films a heavy and a light hit on a goblin. `-MapShotEmitters=<list>` plays particle effects one by one to compare them.

## Looking at it

These all need a GPU and a window:
- `-HellgirlMapShot` on any map saves the player's view and an overview to `Saved/Screenshots/MapShot`.
- `-HellgirlForestRunPreview` shows forest rooms. Add `-PreviewMeshes=/Game/A.A;/Game/B.B` and `-PreviewMaterials=...` to line up assets in the forest lighting.
- `-HellgirlHubPreview` shows the camp.

Still primitive: the castle stages (walls, gates), the old lava and astral maps, the bedroll, and the purple exit portal on the castle maps.
