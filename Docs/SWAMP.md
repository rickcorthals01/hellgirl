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
| **I, The Swamp of Souls** | One stretch of swamp (always seed 1101). A six-second camera flight from the far end back to Hellgirl, then the Goblin Queen's intro. Ten waves of rats and frogs, each arriving ahead of Hellgirl along the corridor. Conversations after waves 2, 6 and 9, then the purple portal by the light at the end. |
| **II, Deeper In** | A run of ten rooms (a new seed each run), one wave per room; the light only opens once the room's wave is beaten. Health, energy and Souls carry over, and dying ends the run. Her lines play at rooms 1, 5, 7 and 10. Room 10 is the Frog King's: he waits on his giant lily pad, guarded by rats and frogs. Beating him brings "Up there! I can see it." and the purple portal. |
| **III, The Doorway** | Two waves (seed 3303), then the doorway conversation, ending "Watch out!". The Rat Queen's fight comes later, in her own area, so for now the purple portal leads back to camp. Her after-fight conversation is already in `LevelTwo.ini` (`S3_AfterRatQueen`). |

Waves (before the global ×1.5 wave scaling):
- **Stage I:** 3 rats; 4 rats; 3+1 frog; 3+2; 3 frogs; 4+2; 5+2; 3+4; 5+4; 6+5.
- **Stage II, room *r*:** 2 + *r*/3 rats and 1 + *r*/3 frogs; room 10 has the Frog King plus 3 and 3.
- **Stage III:** 5+2, then 4+4.

The dialogue file lists "WAVE 1 & 2" then "WAVE 4 & 5 & 6", so its lines are read as coming after waves 2, 6 and 9.

## The rat and the frog

Both come from their rigged Meshy models, with Mixamo clips fitted onto their rigs by `Tools/Enemies/swamp_enemies.ps1` (`rat_clips.json`, `frog_clips.json`). They are imported into `/Game/Enemies/Swamp/<Rat|Frog>`. Their model slots are `Rats`, `Frogs` and `FrogKing` in `Config/DefaultGame.ini`. The tactics are in `Enemies/SwampCombat.cpp`, the moves in `Enemies/EnemyMoveset.cpp`, and the numbers in `Rules/EnemyTuning.h`.

| | Moves |
|---|---|
| **Rat** (fast, 470 cm/s) | **Bite:** a quick 0.45 s lunge for low damage, once every 4 s. A perfect dodge cannot counter it and it shows no counter flash; a normal dodge still avoids it. **Punch:** a very fast 0.55 s charge and release for medium damage, followed 55% of the time by a second punch with the other hand. **Dodge roll:** when Hellgirl winds up an attack at it, a 40% chance (at most every 3 s) to roll aside and away, untouchable while rolling, dropping its own attack if that has not landed yet. |
| **Frog** | Always hopping, fast and high (a 0.3–0.75 s pause between hops): straight at her from afar, around her up close, and up and over her when its slam is ready. **Punch** on the ground (0.45 s, low damage). **Air punch** when level with her mid-hop. **Slam** from high up (every 5 s): a blue circle marks where it will land, then it dives. It deals medium damage in a 3.2 m circle with a small blast that pushes her back; other enemies in the splash take 6 damage and are pushed away. |
| **Frog King** (mini-boss, 750 health, boss bar) | A bigger frog (1.25 × model, 1.5 × actor) that hops higher and slams every 2.8 s in a 4.6 m circle. |

Their clips:
- **Rat:** idle; its own Meshy run; bite (Headbutt); punch and second punch (RightPunch, mirrored); roll (Stand To Roll); hit.
- **Frog:** idle; its own run; hop (Jump, played over its time in the air); punch (RightPunch); air punch (AirPunch); slam (Hellgirl's AirSlam); hit.

New model slots can hold `Attack2`, `AirAttack`, `HeavyAttack`, `Dodge` and `Jump` clips as well as `Attack` and `QuickAttack`. Spawn sites can mix a second kind of enemy into a wave (`MixType`, `MixCount`).

Checks:
- `SwampEnemy` (in the swamp map): the rat bites (uncounterable), punches and rolls away from her swings; the frog hops and slams her.
- `SwampStage` (Stage I, a Stage II room, Stage II room 10 and Stage III): walks each script. It checks the wave counts, that only rats, frogs and the Frog King appear, where the Frog King stands, that the portal or light opens, and that every conversation exists.
- `-HellgirlSwampEnemyPreview` (at camp, windowed): the rat's and the frog's clips at their key moments, saved to `Saved/Screenshots/SwampEnemies_*.png`.
