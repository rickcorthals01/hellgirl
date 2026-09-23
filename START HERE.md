# Hellgirl

A third-person hack-and-slash for Unreal Engine 5.8. Hellgirl is pulled into a hellworld and fights through seven worlds of enemies and bosses, ending with Lucifer.

Last updated September 23, 2026. The project and C++ module were renamed from `AshenArena` (the original template) to `Hellgirl` that day; older docs in `Docs/History` still use the old name.

## Build and play

1. Close Unreal Editor.
2. Run **Build and Open.cmd**. It compiles the game and opens the editor. If it fails, the error is in `Logs\BuildLog.txt`.
3. Press **Play**, then click in the viewport to capture the mouse.

The game opens on the main menu. Once the first stage is beaten, startup goes to the forest camp instead.

All levels are generated in C++ when Play starts. There are no level `.umap` files, so the editor viewport is empty before Play.

## What's playable

| World | Stages | Boss | State |
|---|---|---|---|
| I: Goblin Ruins | First Raid, Survival, The Queen | Goblin Queen | Playable |
| II: Imp Torture Arena | Torture Arena | Imp Commander | Playable (stage 1 only) |
| III: Succubus Court | Map preview (The Court) | Succubus Queen | Map only, no enemies yet: round gothic court, central pool, 8 pillars, 4 sealed arches |
| IV–VII: Ghosts, Rats, Frogs, Apostles | — | Ghost King, Rat Queen, Frog King, Lucifer | Planned |
| Final special stage | — | Lucifer (Devil Form) | Planned |

**Forest camp hub** (unlocked after World I, Stage I): campfire to heal and change outfit, and a road to the world/stage select. After the Goblin Queen is beaten, a goblin merchant appears; the shop itself is not implemented yet.

**Weapons:** fists and sword. The gun and car were removed from the design on September 23.

**Outfit ultimates:** Rags, Goblin Queen and Succubus Armor work. Other outfits have no ultimate yet.

## Controls

| Action | Keyboard / mouse | Controller |
|---|---|---|
| Move / look | WASD / mouse | Left stick / right stick |
| Light attack | Left click | X |
| Heavy attack (hold to charge) | Right click | Y |
| Dodge | Shift | B |
| Jump | Space | A |
| Sprint | F | Left stick click |
| Walk | Left Ctrl | — |
| Outfit ultimate | Q | Right stick click |
| Fists / sword | 1 / 2 | D-pad up / right |
| Interact | E | Y |
| Camera zoom | Mouse wheel | Triggers |
| Pause | Escape or P | Menu |
| Restart map | R | — |

Keyboard bindings can be changed in **Pause > Options**. Controller bindings are fixed.

## Known gaps

- Animated so far: idle, walk, run, both fist combos, charge and charged strike, sword slash/backslash/spin. The after-dodge moves, air combo and reactions still use the neutral pose until their Mixamo clips are added (see **Docs/ANIMATIONS.md**). The sword is a placeholder box.
- The shop, the Succubus Hall hub, World III's encounters and Worlds IV–VII are not built yet.
- Balance values are initial tuning and have not been hands-on playtested.

## Where things are

| Path | Contents |
|---|---|
| `Source/Hellgirl/Fighter/` | `ArenaFighter`: the player and all enemies (movement, combat, animation) |
| `Source/Hellgirl/Bosses/` | One component per boss (Goblin Queen, Imp Commander): phases, summons, shields |
| `Source/Hellgirl/Enemies/` | Enemy types, movesets, spawn points, projectiles |
| `Source/Hellgirl/Levels/` | `ArenaGameMode`: builds every map in code and runs progression, hub and story |
| `Source/Hellgirl/UI/`, `Progress/`, `Rules/`, `Checks/` | HUD and menus; wallet, saves and coins; pure combat rules; automated checks |
| `Config/DefaultInput.ini` | Default key bindings |
| `Content/` | Imported models, animations, textures and materials |
| `Tests/` | `run-checks.ps1` runs every automated check (about 5 minutes, editor closed); plus standalone rule tests |
| `Tools/Animations/` | Mixamo → outfit animation pipeline (see Docs/ANIMATIONS.md) |
| `Docs/` | Current design and system notes (see below) |
| `Docs/History/` | Older playtest and build notes, kept for reference; they may describe removed features |
| `Logs/` | Build and test logs (not tracked by git) |

### Docs

- **HELLGIRL DESIGN.md**: overall concept, level progression and the apartment intro.
- **HELLGIRL COMBAT.md** and **COMBAT UPDATE.md**: combat design and the current move list, energy and ultimates.
- **CAMPAIGN.md**: boss mechanics. Its level numbering predates the World/Stage select.
- **FOREST HUB.md**, **DIALOGUE.md**, **PAUSE MENU.md**: those systems.
- **ARENA LOOP.md**, **STAGE ONE MAP.md**: castle arena layout and the older lava and astral maps, which are still in the code for development.
- **NATIVE MODELS.md**: how the character models were set up.
- **ANIMATIONS.md**: adding Mixamo combat clips and previewing them.

The original design notes, reference art and source models live outside this folder in `Desktop\Hellgirl Game`, mainly `Developer idea folder lol\`.

## Version control

This folder is a git repository, backed up to a private GitHub repo. Large assets (`.uasset`, models, images, audio) are stored with Git LFS.
