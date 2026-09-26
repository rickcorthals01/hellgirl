# The swamp (World II, map preview)

World II starts in a dark swamp of glowing soul water. Open it from the forest road: **World II → THE SWAMP / MAP PREVIEW**. There are no enemies yet; the rats and frogs come later. The Rat Queen gets her own boss area later, and the Frog King is this map's mini-boss.

- A run is four rooms, each a stretch of the same swamp. Walk into the light at the east end to go on. The light in room 4 leads back to camp.
- Room 4 is the Frog King's: his giant lily pad floats in the middle of the water.
- R restarts the current room. Runs cannot be saved.

## The corridor

The corridor is 125 × 28 m, the length of the first goblins map. An unbroken ring of dead trees surrounds it on all four sides, several rows deep, with an invisible wall just inside. West to east:

| Zone | Length | What is there |
|---|---|---|
| Muddy grass | about 0–25 m | Only a few dead trees (2–4). |
| Soul water | about 25–100 m | Shallow, glowing light-blue water that Hellgirl wades and fights in. It holds a rotten boardwalk winding across (partly broken), mud islands, drowned stumps and trees, reeds, dark lily pads (only on the water) and zombie arms. |
| Mud | about 100–125 m | Only a few dead trees, and the light at the end: a lantern on a crooked post with a warm glowing orb. |

## What is random

Each room rolls its contents from the run's **seed**. The same seed and room always give the same layout. Rolled per room:
- where the water starts and ends (±4 m);
- the boardwalk's route;
- the islands, stumps, drowned trees, reeds, lily pads and zombie arms;
- the few trees in the mud;
- 5–8 swarms of fireflies (pure decoration).

The rules:
- The mud holds dead trees and nothing else.
- The way in and the light stay clear.
- Lily pads and reeds don't pile on each other or on the boardwalk.
- Zombie arms stay 3.2 m from the boardwalk's line, so the boardwalk is always a safe way across, and 4.8 m from each other.
- A walk test checks that the light can be reached from the way in without passing within an arm's reach.

The rules and the planner live in `Source/Hellgirl/Rules/SwampRules.h`. The room is built in `Levels/Swamp.cpp` and dressed in `Levels/SwampScenery.cpp`.

## Water, splashes and zombie arms

- **Wading:** the water surface is 18 cm above the floor, so feet wade just under it.
  - Moving through the water leaves ripple rings at her feet (every 0.28 s), with droplets when running.
  - A landing or a dodge in the water throws a big splash.
  - Arms throw a splash when they rise.
- **Zombie arms:** glowing soul-blue arms, 1.65 m high when risen.
  - Each one rises out of the water, reaches and claws (the fingers flex), sways, sinks, and stays under for a while. The cycle is 7.5 s, and each arm has its own phase and speed.
  - While an arm is risen, anyone within 1.7 m of it takes 6 damage every half second ("DROWNED HANDS!").
  - A sunk arm is harmless.

## Look

- **Light:** an overcast night with no moon and thick low green-grey fog. The soul water is the brightest thing in the swamp, and soft blue glows along it light the trees and Hellgirl from below.
- **Trees:** dead swamp trees with flared roots and hanging moss, mixed with the forest kit's dead trees.
- **Water material:** `M_SwampWater` is a deep blue base glowing light blue, with bright veins where two drifting noise layers cross.
- **Other materials:** `MI_SwampSoul` gives the arms their glow, `M_SwampRipple` draws the splashes and the exit orb's halo, and `MI_SwampGround` is the muddy grass.

The swamp kit is generated in Blender, low-poly with vertex colours: three swamp trees, the zombie arm, two lily pads, the giant lily pad, the boardwalk and its broken version, reeds, a drowned stump, a mud island, the ripple ring and the exit lantern. Rebuild the art with the editor closed, after the graveyard art (it reuses its noise texture, ground textures and ground material):

```
powershell -ExecutionPolicy Bypass -File Tools\Environment\build_swamp_kit.ps1
```

## Testing

- `-HellgirlSwampCheck` plans 1,600 rooms (400 seeds × 4 rooms) and checks that each is fair and repeatable and that the rooms vary. It then checks:
  - the built room matches its plan, the boardwalk can be stood on, and there is a floor at the way in;
  - a risen arm hurts and a sunk one doesn't, and splashes appear;
  - the light only takes Hellgirl on once she reaches it.
- Play a specific room: `/Engine/Maps/Entry?Swamp=1?Seed=4242?Room=4`.
- `-HellgirlSwampPreview` (windowed, needs a GPU) saves eight views to `Saved/Screenshots/Swamp`, with the arms held up: from above, the way in, the water's edge, across the water, a zombie arm, back over the water, the light, and the play camera.
