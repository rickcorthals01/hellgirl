# Local setup status

## Current — September 15, 2026

The latest balance pass reduces charged strikes to 12–28 base damage, aerial slam to 16 and crash kick to 14. Area damage is full inside the inner quarter of the radius and falls to 55% at the edge. All launches caused by one area attack share a collision limit of 6 total damage per victim, including chains and a captured flyer's descent before landing. A fully charged strike with maximum riposte and its entire collision allowance deals at most 62 damage, below a fresh Imp's 67 HP. Launch force, radius, basic attack damage, energy gains and costs of 40/35 remain unchanged.

All three bosses take damage during attacks but resist stagger, knockdown, launch and interrupting dodge counters until the action ends. The Commander's summon and recovery have the same protection. Timed dodges still evade normally; idle bosses can stagger, and lethal damage or a reset can stop an action. The standalone combat-rule test, editor build and runtime combat balance, enemy moveset, energy, physics, counter and three-map progression checks passed. See ENERGY PLAYTEST.md and PHYSICS PLAYTEST.md.

The castle enemy moveset is implemented in source. Ground Imps circle, pounce and claw; FlyingImps commit to dives and recover near the floor. The Imp Commander has cleave, rush and circular slam attacks, plus a permanent second phase at 50% health. His summon has a 1.8-second windup and 1.2-second recovery, an 18-second base cooldown and a cap of three living summoned Imps. Surviving reinforcements remain part of the boss encounter and must be cleared before the portal opens. The HUD shows boss health, phase and summon warnings. Shared attack spacing limits nearby castle enemies to two simultaneous attacks, at least 0.45 seconds apart. See ENEMY MOVESET PLAYTEST.md for controls and initial tuning.

Before the latest balance change, the editor build and initial enemy checks passed phase transition, summon spawning/cap, the previous interruption rules, encounter tracking, rewards, movement, counters and attack coordination. The full three-map journey with energy carryover/reset, plus physics, energy and counter regressions, also passed again. A rendered preview verified phase 2 with three reinforcements and the boss HUD. Ground Imp pounce selection now stops at 550 cm to stay within its movement and hit reach. Expanded contact checks also passed all five new damaging moves, single-hit timing, rear/range/wall coverage for the slam and the blocked rush. The existing Imp/FlyingImp animation assets and placeholder Commander remain in use; hands-on balance and readability are also pending.

### Earlier September 15 scenery pass

The Stage 01 scenery pass is implemented and saved. Castle ruins now have raised terrain banks, a level combat courtyard and encounter pads, a blended winding earth path, individual broken masonry, boulders, small nonblocking clutter and distant rocks. Lava and astral platforms retain their level rectangular top footprints above tapered, uneven rock undersides; larger islands have basalt or glowing crystals, and the astral route has floating fragments below it. Fog, lighting and ambient occlusion add depth. The moving lava surface sits at Z = -180 cm so standing in it triggers the existing damage rule. Bridge stones sit 3 cm lower to prevent coplanar flicker.

Stone, plaster and iron textures and the editable Surface, Earth and Glow materials are saved in `/Game/Environment/Textures` and `/Game/Environment/Materials`. Iron is imported but unused. Level geometry is still generated at play by `ArenaGameMode.cpp`, `MapVisuals.cpp` and `CastleTerrainLayout.h`/`StageOneTerrain.cpp`; separate `.umap` layouts and a painted Landscape are future work. Imps, FlyingImps and Gulps have integrated meshes; other types retain placeholders. Health and energy HUD placement at the bottom of the screen is also complete. See STAGE ONE MAP.md.

Checks completed during this pass: full editor compilation; terrain floor, spawn and capsule clearance; actual entrance traversal; the complete three-map journey with energy travel/reset; and physics collision, ragdoll and reward regression. Manual combat balance, every platform jump and the final feel of the scenery remain for hands-on playtesting.

## History — September 14 combat and physics updates

Latest combat update: a separate 100-point energy meter now funds charged attacks (40) and aerial slams/crash finishers (35). Successful basic normal/aerial hits earn 12, and uncharged heavy hits earn 18, once per swing. Big moves retain their damage and knockback. Fresh games and restarts begin empty; energy carries through forward portals. See ENERGY PLAYTEST.md.

The energy build and runtime checks passed, including actual hit rewards, payment and rejected-input rules, aerial fallback, buffering and cancellation. The full three-map journey verified portal carryover and restart reset; movement and dodge-cancellation checks passed. The new meter was visually checked in game. Balance remains ready for hands-on feedback.

The physics system is implemented in the local Unreal project: momentum-driven leaps and charges, strong-hit enemy launches, collision damage and chain reactions, and death ragdolls with a placeholder fallback. Corpses remain for 12 seconds of unpaused game time; encounter credit and coins are granted once. See PHYSICS PLAYTEST.md and COMBAT MOVEMENT.md for controls and initial tuning.

The physics build compiled under Unreal Engine 5.8.2. Runtime collision and corpse checks passed at 30 and 120 fps after the earlier 60 fps baseline, including wall impacts, chain limits, speed thresholds, recovery, four corpse variants, and lethal-impact rewards. Physics pause/resume, the complete three-map journey, dodge cancellation and counters also passed. All four animation tests passed with zero reported errors.

Movement range, ledges, finite airborne boosting and travel distance across frame rates passed their checks. Actual lunging-punch damage passed at 30, 60 and 120 fps and during a 200 ms frame delay; walls prevent damage. The timing fix waits for the final approach movement before resolving contact. Manual balance and visual playtesting remain pending. The notes below describe earlier milestones and are retained as history, including old controls and build limitations that no longer describe the current game.

## History — earlier setup notes

Verified September 12, 2026.

Enemy integration: spawned enemies now have named types and visible labels. Project Settings > Game > Hellgirl Enemy Models provides separate skeletal mesh and animation slots for Imps, FlyingImps, ImpCommander, Gulps, GulpBoss, Succubus, and SuccubusBoss. Empty slots retain the placeholders. See ENEMY MODELS.md.

Latest combat update: enemy hand flashes signal a 0.3-second perfect-dodge window, extended from 0.18 seconds after playtesting. A correctly timed dodge counters one threatening attacker, deals damage, and launches them backward. See PERFECT COUNTER.md.

Latest: Stage 01 now has three runtime-generated maps (castle ruins, lava islands, astral paths), placeholder bosses, gated encounters, portal prompts and travel, lava damage, and void recovery. Full Unreal compilation and unattended checks passed, including the complete map 1-to-3 journey and final stage completion. See STAGE ONE MAP.md. Manual traversal, combat balance, and visual playtesting remain pending.

Current status: the full AshenArenaEditor Win64 Development build succeeded with Unreal Engine 5.8.2 on September 12. This includes the landscape/rifts, combat, character animation integration, coins, and the new block/riposte/sprint pass. Standalone defense-rule checks passed. In-game verification of this latest pass remains pending; see BLOCK AND RIPOSTE PLAYTEST.md. The earlier build-pending notes below are historical.

### Earlier status notes

Latest source: Stage 1 landscape and three proximity-activated rifts replace the room and global waves. Aerial damage is now 12/12/18/28 and coins attract gradually but require contact. Standalone stage, coin-contact, and combat rule checks passed. Full Unreal compilation and map playtesting remain pending; see STAGE ONE MAP.md.

Latest source update: hold/release heavy charge and a four-step aerial normal combo, including flying-target dashes and a landing-impact finisher. Expanded standalone C++ combat-rule tests passed. Full Unreal rebuild and gameplay verification for this pass remain pending; see CHARGE AND AIR PLAYTEST.md.

Latest update: controller vertical look flipped; enemy coin pickups and a persistent local wallet added in source. Exhaustive C++ coin-drop tests passed (300 input combinations, exactly 5% rare). Full Unreal rebuild, camera check, and wallet save/reload playtests remain pending. See COINS PLAYTEST.md.

Latest source update: first fist-combat pass is saved; see FISTS PLAYTEST.md. Move-selection tests compiled with the installed MSVC compiler and passed. The full Unreal build and gameplay verification for these new changes remain pending. The binaries described below are from the earlier baseline and movement-fix builds.

- Unreal Engine 5.8.2 is installed.
- Visual Studio Community 2026 and the C++ compiler component are installed.
- The game and editor targets now use Unreal 5.8 build settings (V7) and include order.
- The user-triggered rebuild produced UnrealEditor-AshenArena.dll and the editor target receipt at 00:00 local time.
- Unreal Editor successfully initialized and loaded the Entry map. Its startup map check reported zero errors and zero warnings.
- Unreal Editor is running. Gameplay has not yet been verified.

Current project: Desktop\Hellgirl Game\Unreal Project\AshenArena.uproject

The running build is the generic arena scaffold. Controls currently remain WASD movement, mouse look, left-click sword attack, Space dodge, and R restart. The Hellgirl-specific intro, unarmed moves, controller controls, and progression are documented in HELLGIRL DESIGN.md and HELLGIRL COMBAT.md but are not yet implemented.

Earlier automated build attempts were blocked by this session's access to UnrealBuildTool's local settings folder. The successful build was launched by the user through Unreal Editor.
