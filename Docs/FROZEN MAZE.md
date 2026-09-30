# The Frozen Maze (World IV, map preview)

The Deprived's world is an icy underground cave built as one big maze. Open it from the forest road: **World IV → THE FROZEN MAZE / Map preview**. There are no enemies yet; the Deprived come later.

- Each run starts in one of the four spawn rooms, picked at random. Find the vortex in the middle of the maze; stepping into it leads back to camp.
- R restarts from the same spawn room. After a fall, TRY AGAIN starts a new run from a new random room.
- There is no minimap here: finding the way is the point.

## The maze

It is the largest map yet: 38 × 26 squares of 8 m (304 × 208 m).

- **Walls:** jagged ice, 9–14 m tall, so you can't see over them. Every corridor and doorway is one square wide (about 6.5 m between the walls).
- **Outer wall:** rises all the way to the cave roof, 28 m up.
- **Rooms:** the four spawn rooms (S1–S4, one door each) and the vortex room. The vortex room's door is at its north-east corner.
- **Walks:** the shortest walk from each spawn room to the vortex is about 456 m (S1), 328 m (S2), 440 m (S3) and 552 m (S4).

The approved design is in `Developer idea folder lol\Maze design plan.png`.

| Element | Where | What it does |
|---|---|---|
| **Waterfalls W1–W5** | Down the north and south outer walls | Fall from cracks in the roof into shallow pools, each lit by a shaft of pale light |
| **Curtains C1–C8** | Across gaps in maze walls | Walk-through water falling from the roof; each hides a way through a wall |
| **Glow crystals** (26) | Against a wall at junctions | The only light between the rooms |
| **Frozen remains** (12) | In dead ends | A victim frozen in a block of clear ice. Attack it to smash it: 12–18 Souls spill out |
| **Icicle traps** (8) | Straight stretches on the routes to the vortex | Walking under the ice bridge shakes its big icicle loose. Its shadow grows on the ice for 0.9 s, then it falls: 18 damage and a knock back. A dodge or block avoids it, and a new icicle grows back after 20 s |
| **The Deprived's lairs D1–D16** | Dead ends, none near the spawn rooms | Pools of black shadow ringed by dark shards and old bones, with a violet glow. The Deprived will rise from them one at a time (see below) |
| **The vortex** | Over the frozen lake in the vortex room | A turning purple spiral; the lake's cracks glow purple around it |

## The Deprived

Strong shadows that hunt Hellgirl through the maze, one at a time. The behaviour is in `Enemies/DeprivedCombat.cpp`, the numbers in `Rules/EnemyTuning.h`, and the maze's side in `Levels/MazeHunt.cpp`.

- **Rising:**
  - The first rises 6 s after she arrives.
  - When one falls, its Souls drop and the next rises 10 s later.
  - Each rises from the nearest lair she can't see that is at least 5 squares' walk (40 m) from her.
- **Hunting:** it always knows where she is. In sight it comes straight at her; out of sight it follows the maze. A few times a second the maze works out the walk to her from every square, and the Deprived heads for the next square along it.
- **Health:** twice an ordinary enemy's (118).
- **Speed boost:** doubles its walking speed (3.8 → 7.6 m/s) for 6 s, then 5 s before it can boost again. It boosts whenever it chases her from further off than 5 m or out of sight.
- **Execute:** once per life, when it reaches her. It charges for 0.5 s, then lunges through her for 40 damage and knocks her down. A dodge avoids it; a perfect dodge parries it (her counter). It glows violet while charging.
- **After the execute:** slow slices and claws, 1.2 s each, for medium damage (15 and 13).
- **Model:** the user's Meshy model (`Meshy Models\Enemies\Deprived enemy model`), rigged onto Hellgirl's own skeleton, so it plays her sword clips (idle, run, slash for the slice, backslash for the claw, charged strike for the execute, hit, death). It holds a black blade for now (a scythe later) and has a violet rim of light (`M_Deprived`). Its model slot is `Deprived` in `Config/DefaultGame.ini`; if emptied, a stand-in (Hellgirl's body in black) returns.

### Rebuilding the Deprived model

Close the editor, then:

```
powershell -ExecutionPolicy Bypass -File Tools\Enemies\deprived.ps1
```

1. `rig_deprived.py` (Blender) does the rigging:
   - It reduces the model to 60k triangles and fits it onto Hellgirl's Rags body. Each arm is fitted on its own, because the model's two arms hang differently.
   - The skin takes her body's weights. Hair follows the skin it grows from, fading into the head and spine.
   - The few faces where Meshy fused hair to the arms are cut, so the hair doesn't stretch into sheets when the arms swing.
   - Renders go to `Animation Testing\Enemies\Deprived\Preview`:
     - `fit`: the model over her body;
     - `skin`: red for skin, grey for hair;
     - `rest` and `posed`: a test pose.
2. The textures are copied at 2048.
3. `import_deprived.py` (Unreal) imports `/Game/Enemies/Deprived` on `/Game/Hellgirl/Outfits/Rags/Rags_Skeleton` with its material and physics asset. Her skeleton is left untouched.

The check is `-HellgirlDeprivedCheck` (Deprived in `Tests/run-checks.ps1`), run in a real hunt. It covers:
- the rise rule;
- twice the health;
- the hunt through the maze;
- the boost's length, speed and cooldown;
- the 0.5 s charge and the parry;
- slice and claw after the execute;
- the next rise 10 s after one falls;
- the execute's 40 damage.

`-HellgirlDeprivedPreview` (needs a GPU) photographs one up close.

## The boss room (room only)

The cavern beyond the vortex, built from the user's sketch (plan: `Developer idea folder lol\Boss room design plan.png`, drawn by `Tools/Maze/boss_plan.ps1`). Open it from the main menu: **DEV → MAZE BOSS ROOM**, or `/Engine/Maps/Entry?MazeBoss=1`. No boss yet, and the stages that lead here are not built.

The planned flow:
- Stage 1: the maze, with the vortex leading to camp.
- Stage 2: waves of enemies in the vortex room.
- Stage 3: this room.

**The room:**
- **Size:** a wide open oval, 100 × 70 m, with the roof 45 m up. It is walled all round with ice rising almost to the roof.
- **The imprint:** in the middle, the vortex's charred imprint (34 × 20 m, `M_MazeImprint`). It is a burnt black spiral with grey ash along its seams and embers smouldering at its heart. The ice cracks around it glow faintly like embers.
- **Stalagmites M1–M4:** huge ice spires, 19–25 m tall, placed as in the sketch. Each stands under a stalactite hanging from the roof, and they block like pillars.
- **Frozen bodies R1–R6:** they smash for Souls like the maze's frozen remains.
- **Purple lightning:** 22 bolts (`M_MazeBolt`), each of which strikes, flickers out, and strikes again 1–5 s later along a new jagged path, with a violet flash of light:
  - 12 crawl along the walls;
  - 6 arc across the roof between the stalactites and out over the imprint;
  - 4 jump between each stalactite and the stalagmite below it.
- **Light:** nine glow crystals round the wall and one at each stalagmite. The fog is thinner than in the maze, so the far side shows.
- **Start and boss:** she starts west of the imprint, facing it. The boss's place, east of the imprint, is kept clear. There is no minimap.

**Where it lives:**
- Rules: `Rules/MazeBossRules.h`.
- Build and lightning: `Levels/MazeBossRoom.cpp`.
- Check: `-HellgirlMazeBossCheck` (MazeBoss in `Tests/run-checks.ps1`). It covers the start, the floor, the closed wall, the blocking stalagmites, the clear imprint and boss place, smashing a frozen body, and every bolt striking and fading.
- Screenshots: `-HellgirlMazeBossPreview` saves them to `Saved/Screenshots/MazeBoss`.

## Look

- **Ice:** glossy, with a cold rim of light on its edges (`M_MazeIce`).
- **Floor:** cracked frozen ice with frost patches; some stretches are crazed with faintly glowing cracks (`M_MazeFloor`).
- **Frozen lake:** under the vortex, dark and clear, its cracks glowing purple.
- **Light:**
  - Faint cold light filters down through the ice roof; it is the only light that casts long shadows.
  - Crystals, pools, waterfalls and the vortex glow.
  - The rest is darkness and thick blue fog that swallows the corridors a few turns ahead.
- **Mist:** three drifting layers hang over the ice.
- **Roof:** dark rock hung with stalactites of ice.

## Art

The kit is generated in Blender, low-poly with vertex colours like the other kits:
- Three ice-wall variants and the pillar where walls meet.
- Two ice spires, the glow crystal cluster, roof icicles and ice rubble.
- The ice bridge and big icicle of the traps.
- The frozen remains and the shadow pool.

Rebuild it with the editor closed (it needs the graveyard and swamp art first):

```
powershell -ExecutionPolicy Bypass -File Tools\Environment\build_maze_kit.ps1
```

This runs `maze_kit.py` in Blender (preview at `Saved/MazeKit/Preview.png`). It then runs `import_maze.py`, which makes these materials in `/Game/Environment/Maze` and imports the kit:

| Material | Used for |
|---|---|
| `M_MazeIce`, `MI_MazeCrystal`, `MI_MazeShadowGlow` | The ice kit, the crystals' glow, the lairs' violet edge |
| `M_MazeIceShell` | The clear ice round the frozen remains |
| `M_MazeFloor` | The cracked floor and the frozen lake |
| `M_MazeFall` | Waterfalls and curtains |
| `M_MazeVortex` | The vortex |
| `MI_MazePool` | The pools |

Reused from the packs:
- The Inferno pack's bones, round the lairs.
- The starter VFX pack's ice burst and glass shatter, for the traps and remains.
- The graveyard's mist material.
- The swamp's water and ripple materials, for the pools and the traps' shadows.

The walls have no collision of their own. The level puts a hidden box along each wall and at each pillar, which is smoother for the camera than jagged ice.

## Changing the layout

The layout is designed in `Tools/Maze` and exported to `Source/Hellgirl/Rules/FrozenMazeLayout.h`:

1. `gen_full.ps1 -Seed 338` rebuilds the walls (`edits.txt`) and curtains (`curtains.txt`). The upper-left quarter is the approved hand-made maze; the rest is generated to match.
2. `elements.ps1` places the lairs, remains, crystals and traps (`elements.txt`).
3. `plan_full.ps1` draws the plan (`Saved/Maze/MazeDesign.png`).
4. `export_maze.ps1` writes the header. Rebuild the game afterwards.

The rules (8 m squares, rooms, waterfalls, trap and remains numbers) are in `Rules/FrozenMazeRules.h`. The map is built in `Levels/FrozenMaze.cpp` (vortex, traps, remains, check) and dressed in `Levels/FrozenMazeScenery.cpp`.

## Testing

- **`-HellgirlMazeCheck`** (in `Tests/run-checks.ps1` as Maze and Maze4) checks:
  - Every square is reachable, so every spawn room leads to the vortex.
  - The walls match the layout; the curtains are open.
  - Lairs and remains are in dead ends; traps are on straight stretches.
  - Every wall is built and blocks, and open sides and curtains let her through.
  - She starts in her spawn room facing its door.
  - Attacking frozen remains smashes them and drops Souls.
  - A trap falls, hurts her and grows back.
  - The vortex only takes her once she steps into it.
- Play a specific spawn room: `/Engine/Maps/Entry?Maze=1?Spawn=0` (0–3 = S1–S4).
- **`-HellgirlMazePreview`** (windowed, needs a GPU) saves eleven views to `Saved/Screenshots/Maze` and logs the average frame time. The views are: the spawn room, a long corridor, a crystal junction, a curtain, the big waterfall, the vortex room, a lair, frozen remains, a trap, the whole maze from above, and the play camera.
