# The swamp (World II)

World II is a dark swamp of glowing soul water, fought through in three stages (see Stages below). The Goblin Queen travels with Hellgirl and talks along the way. The Rat Queen gets her own boss area later; the Frog King is the swamp's mini-boss. The map on its own (no enemies) is still reachable with the URL option `Swamp=1` (Stage 0, four rooms).

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

## Stages

Open them from the forest road, under **World II**. Stage I opens once World I is won (`Stage3Won`); each stage won opens the next (`SwampStage1Won`, `SwampStage2Won`, `SwampStage3Won`). The scripts follow "05 Swamps Stage 1.txt", "06 Swamps Stage 2.txt" and "07 Swamps Stage 3.txt". The conversations are in `Content/Dialogue/LevelTwo.ini`. The stage logic is in `Levels/SwampStages.cpp`.

| Stage | What happens |
|---|---|
| **I, The Swamp of Souls** | One stretch of swamp (always seed 1101). A six-second camera flight from the far end back to Hellgirl, then the Goblin Queen's intro. Ten waves of rats and frogs from the areas' points (below). Blue portals open after waves 2, 5, 7 and 9: stepping in plays her conversation (none at wave 7), then the portal menu (continue, stock Souls, upgrades). After wave 10, the purple portal by the light at the end. |
| **II, Deeper In** | A run of ten rooms (a new seed each run). Each room sends groups of random rats and frogs from three to five of the areas' points (all five in room 10). Once they are all beaten, a blue portal opens (not in room 10), and continuing through it opens the light. Health, energy and Souls carry over, and dying ends the run. Her lines play at rooms 1, 5, 7 and 10. Room 10 is the Frog King's: he waits on his giant lily pad, guarded by rats and frogs. Beating him brings "Up there! I can see it." and the purple portal. |
| **III, The Doorway** | In the boss arena (see Boss arena below). Wave 1 (5 rats + 2 frogs) → blue portal → a bigger wave 2 (6 + 5) → blue portal, and once she continues, the doorway conversation ending "Watch out!" → wave 3 springs the ambush (4 + 4) → blue portal → the Rat Queen leaps in before the crypt. Beaten, her after-fight conversation (`S3_AfterRatQueen`) plays, then the crypt entrance is the way out (walking in wins the level). |

### Where the waves come from

Every wave comes from a fixed point in one of the room's five areas, and only once Hellgirl gets within 13 m of it (or passes it) along the corridor; the next wave waits until the last one is beaten. Each area has a point on the north and the south side (5.5 m off the middle). West to east:

| Area | Where | Stage I (rats + frogs, before the global ×1.5 wave scaling) |
|---|---|---|
| 0 | the first mud, 62% of the way to the water | rats only: 3, then 4 |
| 1 | the start of the water (18% across it) | mostly rats: 3+1, then 4+2 |
| 2 | the middle of the water | frogs only: 3, then 4 |
| 3 | where the water nears the mud again (82% across) | mostly frogs: 2+4, then 2+5 |
| 4 | the last mud, 45% of the way to the light | rats and frogs: 4+4, then 6+5 |

- **Stage II, room *r*:** groups at 3 of the areas in rooms 1–3, 4 in rooms 4–6, 5 in rooms 7–10. Each group is one or two enemies before scaling (two more often later in the run, always two in room 10), a random mix of rats and frogs. In room 10 the Frog King joins the group from the middle of the water.
- **Stage II save point:** room 6. Falling there or later, TRY AGAIN starts the same run again from room 6, at full health with no Souls or upgrades; earlier falls start a new run.
- **Stage III:** 5+2 from the start of the water, then 4+4 from where it nears the mud.

The dialogue file places the blue portals after waves 2, 5, 7 and 9 (wave 7's has no conversation).

## The rat and the frog

Both come from their rigged Meshy models, with Mixamo clips fitted onto their rigs by `Tools/Enemies/swamp_enemies.ps1` (`rat_clips.json`, `frog_clips.json`). They are imported into `/Game/Enemies/Swamp/<Rat|Frog>`. Their model slots are `Rats`, `Frogs` and `FrogKing` in `Config/DefaultGame.ini`. The tactics are in `Enemies/SwampCombat.cpp`, the moves in `Enemies/EnemyMoveset.cpp`, and the numbers in `Rules/EnemyTuning.h`.

| | Moves |
|---|---|
| **Rat** (fast, 470 cm/s) | **Bite:** a quick 0.45 s lunge for low damage, once every 4 s. A perfect dodge cannot counter it and it shows no counter flash; a normal dodge still avoids it. **Punch:** a very fast 0.55 s charge and release for medium damage, followed 55% of the time by a second punch with the other hand. **Dodge roll:** when Hellgirl winds up an attack at it, a 40% chance (at most every 3 s) to roll aside and away, untouchable while rolling, dropping its own attack if that has not landed yet. |
| **Frog** | Always hopping, fast and high (a 0.3–0.75 s pause between hops): straight at her from afar, around her up close, and up and over her when its slam is ready. **Punch** on the ground (0.45 s, low damage). **Air punch** when level with her mid-hop. **Slam** from high up (every 6 s): a blue circle marks where it will land, then it dives. It deals medium damage (all frog hits are scaled by 0.85) in a 3.2 m circle with a small blast that pushes her back; other enemies in the splash take 6 damage and are pushed away. |
| **Frog King** (mini-boss, 750 health, boss bar) | A bigger frog (1.25 × model, 1.5 × actor) that hops higher and slams every 2.8 s in a 4.6 m circle. |
| **Rat Queen** (the boss, 1600 health, boss bar) | Her HD model, rigged onto her old skeleton, 1.5 × actor. Very mobile (520 cm/s), short cooldowns. A string of fast **right slash** and **left slash** (low damage), then a charged **heavy slash** that dashes her up to 7.5 m forward (heavy damage; the charge-up gives time to dodge); the heavy slash alone closes the distance from afar. A **jump slam** now and then (every 7 s at most), its landing circle shown before she hits. A **double dodge roll** when Hellgirl swings at her (every 2 s at most, 70%). At **half health**: "My soldiers! Aid me!" (`S3_RatSoldiers`), her summon pose, and two waves of rats (4 each before scaling) beside her; she keeps fighting, and her wave ends when she and her rats are down. |

Their clips:
- **Rat:** idle; its own Meshy run; bite (Headbutt); punch and second punch (RightPunch, mirrored); roll (Stand To Roll); hit.
- **Frog:** idle; its own run; hop (Jump, played over its time in the air); punch (RightPunch); air punch (AirPunch); slam (Hellgirl's AirSlam); hit.

New model slots can hold `Attack2`, `AirAttack`, `HeavyAttack`, `Dodge` and `Jump` clips as well as `Attack` and `QuickAttack`. Spawn sites can mix a second kind of enemy into a wave (`MixType`, `MixCount`).

Checks:
- `SwampEnemy` (in the swamp map): the rat bites (uncounterable), punches and rolls away from her swings; the frog hops and slams her.
- `SwampStage` (Stage I, a Stage II room, Stage II room 10 and Stage III): walks each script, stepping Hellgirl up to each wave's point. It checks the wave counts, that only rats, frogs and the Frog King appear, where the Frog King stands, that the points run west to east and a wave never starts while she is far from its point, Stage I's areas (rats only in the first mud, frogs only mid-water) and its blue portals after waves 2, 5, 7 and 9, that the portal or light opens, and that every conversation exists.
- `-HellgirlSwampEnemyPreview` (at camp, windowed): the rat's and the frog's clips at their key moments, saved to `Saved/Screenshots/SwampEnemies_*.png`.

## Boss arena (Stage III)

Built from the user's sketch ("Developer idea folder lol/Swamp boss arena.png"); the layout is fixed, in `Rules/SwampArenaRules.h`, and dressed in `BuildSwampArenaScenery` (`Levels/SwampScenery.cpp`).
- **Shape:** a 50 × 50 m square of mud, ringed by dead trees. North is +X, east is +Y.
- **Water:** three streams of soul water run from the south-west to the north-east: a thin one, a middle one widening to the east, and a wide one in the south.
- **Way in:** Hellgirl arrives at the south end of a boardwalk across the wide stream; its last two sections are broken.
- **Way out:** the crypt entrance (the graveyard's `SM_CryptExit`) in the north-west corner, its doorway lit from inside.
- **Pieces:** 9 zombie arms, 4 big dark lily pads to stand on where the upper streams bend, 2 drowned stumps, and one dead tree on the mud. Reeds, small lily pads and fireflies dress the streams. As in the corridor, the mud holds nothing but the tree (and the crypt).
- **Waves:** wave 1 comes from the east mud, wave 2 from the south-west mud, wave 3 from the north-west mud by the crypt; the Rat Queen leaps in just in front of the crypt.
- **Checks:** `SwampStage3` checks the rules above, the blue portal after wave 1 and the crypt opening. `-HellgirlSwampPreview` with `Stage=3` photographs the arena.

### The Rat Queen's model and clips

- **Model:** "Meshy Models/High Quality Models/Rat Queen HD" (1.9 million triangles) is reduced to 150k, fitted onto the old Rat Queen ("Meshy Models/Bosses/Rat Queen") and given its skeleton and skin weights by `Tools/Animations/rig_hq_outfit.py`. The rigged model goes to "Rat Queen HD/rigged/RatQueen.fbx".
- **Textures:** 2048 px copies of the HD textures, including a normal map.
- **Clips** (`Tools/Enemies/ratqueen_clips.json`): idle, her own Meshy run, the right slash (great sword slash), the left slash (the same, mirrored), the heavy slash (great sword slide attack, which hits at frame 40), the jump slam (great sword jump attack, which hits at frame 34), the rats' roll, the summon (great sword power up) and a hit.
- **Rebuild:** with Unreal closed, `powershell -ExecutionPolicy Bypass -File Tools\Enemies\swamp_enemies.ps1 -Only RatQueen`.
- **Portraits:** "Talkbox Images/Rat Queen" → `Portraits/RatQueen` (Angry, Neutral, Talk, Yelling).
- **Check:** `RatQueen` (Stage III jumps to her wave). It checks the slash string, a double roll, her closing in, the summon with two rat waves at half health, and that her hits hurt.
