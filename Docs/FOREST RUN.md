# Forest run (random rooms, level 1)

A first roguelite mode. Start it from the forest road: **World I → FOREST RUN / RANDOM ROOMS**.

- A run is four rooms in the same forest clearing. Rooms 1–3 are goblin fights, and room 4 is the Goblin Queen.
- Clear a room, then walk to the glowing marker at the east end of the trail to go to the next room.
- Health and energy carry over between rooms. Dying ends the run and sends you back to camp; so does beating the Queen.

## What is random

The clearing's shape is fixed: round, with the entrance to the west and the exit to the east. Each room rolls its contents from the run's **seed**. The same seed and room always give the same layout, so a run can be replayed or a bug reproduced.

| Part | Rolled |
|---|---|
| Cover | 4–8 boulders, fallen logs and stumps (2–4 in the Queen's room) |
| Thorns | Patches that hurt while you stand in them: none in room 1 or the boss room, up to one per room number after that |
| Waves | 2–3 waves, each one of: **Goblins** (arrive one by one), **Ambush** (the whole group at once), or **Elites** (1–2 big, tough, fast goblins). Waves get bigger each room. |

Safety rules keep every room fair. The centre (6.5 m) and both doorways stay clear. Cover keeps its distance from other cover, from thorns and from the tree line. Waves never spawn near the entrance. Each room has an enemy budget of 6 + 5 × the room number.

The rules and the planner live in `Source/Hellgirl/Rules/ForestRoomRules.h`; building the room is in `Levels/ForestRun.cpp`. To change what can appear, edit `ForestRoom::Make`.

## Testing

- `Tests\run-checks.ps1 -Only ForestRun` plans 1,600 rooms (400 seeds × 4 rooms). It checks each is fair and repeatable and that the rooms vary, then checks that a fight room and the Queen's room are built as planned.
- Play a specific room: `/Engine/Maps/Entry?ForestRun=1?Seed=4242?Room=2`.
- `-HellgirlForestRunPreview` (windowed, needs a GPU) saves top-down and entrance shots to `Saved/Screenshots/ForestRun`.
