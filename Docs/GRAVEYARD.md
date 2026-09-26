# The graveyard (World IV, map preview)

The ghosts' world starts in a graveyard at midnight under a full moon. Open it from the forest road: **World IV → THE GRAVEYARD / MAP PREVIEW**. There are no enemies yet; the ghosts come later.

- A run is four rooms in the same graveyard. Walk into the glowing crypt in the east wall to go on to the next room. The crypt in room 4 leads back to camp.
- R restarts the current room. Runs cannot be saved.

## The yard

The yard is a 110 m square, the same area as the first goblins map (125 × 95 m). A stone wall with iron railings runs all round it. You enter through the open iron gate in the west, and the crypt is in the east. Two 6 m gravel paths cross in the middle, splitting the yard into four quarters, with a round plaza where they meet.

## What is random

Each room rolls its contents from the run's **seed**. The same seed and room always give the same layout.

| Part | Rolled |
|---|---|
| Quarters | Four different themes out of six: **Old graves** (rows of leaning, weathered stones and obelisks), **Mausoleums** (2–3 small tombs facing the path, flanked by lanterns or mourning statues), **Rose garden** (a hedged square lined with rose beds, an angel in the middle and a rose arch at its opening), **Open graves** (freshly dug pits with dirt piles, shovels and bones), **Crypts** (sarcophagi in a grid, mourners at the corners, candles), **Overgrown** (dead trees, toppled stones, a ruined column, wild roses). Every quarter also gets extra plots of ordinary graves and lone headstones. |
| Crossing | A dead oak, a well or an angel statue in the plaza |
| Mist | How thick the ground mist and the fog are |
| Moon | Where the moon hangs in the east |

Safety rules keep every room fair:
- The paths, the plaza, the gate and the crypt stay clear.
- Each quarter keeps one open clearing (7.5 m radius) for fights.
- Solid pieces keep 70 cm between them.
- A walk test checks that the crypt and all four clearings can be reached from the gate.

The rules and the planner live in `Source/Hellgirl/Rules/GraveyardRules.h`. To change what can appear, edit `FillTheme` / `FillQuarter`. The room is built in `Levels/Graveyard.cpp` and dressed in `Levels/GraveyardScenery.cpp`.

## Look

- **Moon:** a large full moon low in the east, straight ahead as you enter. It has shaded maria, a halo and thin clouds drifting past (`M_GraveSky`). The moonlight is cold silver-blue and casts long shadows back toward the gate.
- **Mist:** three drifting sheets of ground mist (`M_GraveMist`), plus low volumetric fog.
- **Ground:** dark grass on the ground, gravel on the paths and dirt at open graves. These are tinted CC0 textures from Poly Haven (`sparse_grass`, `gravel_ground_01`, `brown_mud_02`), kept in `Desktop\Hellgirl Game\Textures Poly Haven`.
- **Colour:** everything is cold blue-grey. The only warm colour is the red roses. Lanterns and candles burn with pale blue ghost-light.
- **Beyond the wall:** dead trees, dead oaks and dark pines on rising hills.

The graveyard kit is generated in Blender, low-poly with vertex colours like the forest kit. It has 32 pieces: headstones ×6, obelisk, mound, ledger, open grave, sarcophagus, two mausoleums, wall, pillar, gate, crypt, rose bushes, laid roses, rose arch, hedge, dead oak, well, two statues, candles, ghost lantern, bones, grass ×2 and a broken column. Rebuild the art with the editor closed:

```
powershell -ExecutionPolicy Bypass -File Tools\Environment\build_graveyard_kit.ps1
```

This runs `graveyard_noise.py` (tiling mist noise) and `graveyard_kit.py` (the meshes, with a preview at `Saved/GraveyardKit/Preview.png`) in Blender. Then `import_graveyard.py` brings in the textures, the materials (`M_GraveGround` and its instances, `M_GraveMist`, `M_GraveSky`) and the kit into `/Game/Environment/Graveyard`. The solid pieces use their own triangles for collision: headless convex decomposition produces nothing.

## Testing

- `-HellgirlGraveyardCheck` plans 1,600 rooms (400 seeds × 4 rooms). It checks that each is fair, reachable and repeatable, and that the rooms vary. It then checks that the built room matches its plan, that headstones block, and that the crypt only takes you on once you step into it.
- Play a specific room: `/Engine/Maps/Entry?Graveyard=1?Seed=4242?Room=2`.
- `-HellgirlGraveyardPreview` (windowed, needs a GPU) saves eight views to `Saved/Screenshots/Graveyard`: from above, from the gate, into each quarter, the crypt, and across the yard to the moon.
