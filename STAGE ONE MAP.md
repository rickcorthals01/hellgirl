# Stage 01: three-map prototype

Initial progression implemented September 12, 2026 from `Developer idea folder lol/Stage 01 Hands/Stage 01 ALL MAPS.png`. Scenery and castle enemy movesets updated September 15, 2026.

Open the Unreal project and press Play to start at the castle ruins. All three layouts are generated at play using the existing Entry map. Edit layout and scenery in `ArenaGameMode.cpp`, `MapVisuals.cpp`, and `CastleTerrainLayout.h`; `StageOneTerrain.cpp` builds the castle terrain mesh and collision. Separate level `.umap` assets and a painted Landscape are not implemented.

## Map 1: castle ruins

A winding earth path crosses broken masonry into a level combat courtyard. Raised banks surround the route; level pads preserve the entrance, encounters, boss orb and exit. Boulders, small grass tufts, fragments and distant rock silhouettes add detail. Small clutter has no collision, while solid walls and boulders still support combat impacts. Each enemy's spawn height is projected onto the ground.

Approaching the first rift starts four consecutive groups from fixed sites:

1. 3 ground imps.
2. 10 ground imps.
3. 3 flying imps.
4. 10 ground imps and 3 flying imps.

Each group must be defeated before the next emerges. Ground Imps circle, pounce and claw; FlyingImps dive and recover near the floor. After the fourth group, approach the orange fire orb to summon the Imp Commander.

The Commander has 450 health and uses a forward cleave, rushing attack and circular ground slam. At 50% health he enters phase 2 and can summon up to three living ground Imps. The call has a 1.8-second windup and 1.2-second recovery, with an 18-second base cooldown. His active attacks and calls still take damage but cannot be interrupted by hits, launches, physics impacts or dodge counters; lethal damage and a reset can stop them. Defeat both the Commander and any surviving reinforcements to reveal the purple portal to map 2. See ENEMY MOVESET PLAYTEST.md for tuning and checks.

## Map 2: lava islands

Four Gulp islands flank a branching route of floating stepping stones. Four Gulps per island stand ready from the start and engage nearby players. Clear the islands in any order to summon GulpBoss on the far island. Defeat it to open the portal to map 3.

Platforms retain their level rectangular top footprints, with tapered, uneven rock undersides and basalt crystals at the larger islands' corners. The lava has a moving surface pattern. Its surface is at Z = -180 cm so standing in it crosses the existing damage threshold.

Lava deals 25 damage every half second, bypassing block and dodge. Jump back onto a nearby rock. Falling beyond the lava plane returns the player to the last grounded landing. These values are provisional tuning.

## Map 3: astral paths

Purple rock platforms and narrow floating paths cross a dark space setting. Their level top footprints are preserved above irregular, tapered undersides. Glowing astral crystals and suspended fragments beneath the route add depth. Some stretches require jumps. Defeat succubus 1, then succubus 2, then the Succubus Queen (`SuccubusBoss`). The final portal completes Stage 01.

Void falls cost 20 health and return the player to the last grounded landing. Enemies knocked off platforms return to their encounter site so encounters cannot become stuck below the map.

## Controls and progression

- At a fire-orb or portal prompt: E / D-pad Up accepts; N / D-pad Down declines.
- After declining, walk away and approach again to reopen the prompt.
- R restarts the current map, including its encounters.
- The wallet persists across maps. Energy carries through forward portals; fresh games and restarts begin with zero energy. Health and other combat state reset on entering a map.
- Stage completion is displayed for this run; a permanent campaign-unlock save is not implemented.
- Existing attacks, block, riposte, sprint, camera zoom, and character animations remain available.
- All three bosses resist stagger and launches throughout their active attacks; timed dodges evade without countering them. Idle bosses can still stagger.

Hellgirl's latest area balance uses 12–28 base charged damage, 16 slam damage and 14 crash-kick damage. Damage falls from full within the inner quarter of the radius to 55% at the edge. Each area attack allows at most 6 additional collision damage per victim across all of its launches and chains. This preserves crowd displacement without letting one fully charged, maximum-riposte strike erase fresh 67-HP Imps: its upper damage bound is 62. Energy costs remain 40 for charge and 35 for aerial finishers; launch force, radius and basic attack damage are unchanged.

The minimap shows platform footprints, enabled encounters, the player in cyan, and the exit location in purple. The physical exit portal appears only after its boss encounter is cleared, including the Commander's surviving reinforcements.

## Materials and remaining scope

Starter stone, plaster and iron textures are imported and saved under `/Game/Environment/Textures`. Editable `M_EnvironmentSurface`, `M_EnvironmentEarth` and `M_EnvironmentGlow` materials are saved under `/Game/Environment/Materials`. Stone and plaster contribute to the current scenery; iron is available for later use. Fog, warmer castle/lava lighting, cooler astral lighting and ambient occlusion complete this pass. Bridge stones sit 3 cm below the main island tops to prevent overlapping surfaces from flickering.

Imps, FlyingImps and Gulps use their integrated meshes. Other enemy types and bosses retain placeholders. Bosses have 450 health. The Commander already has three distinct attacks and phase-2 summons; unique GulpBoss and SuccubusBoss moves and final boss models remain future work. Enemy counts, attack balance, jump spacing and environmental damage are provisional tuning.

## Verification

The editor target compiled successfully. Checks completed during this pass covered terrain floors, walkable route samples, player-capsule clearance, every encounter's spawn clearance and actual movement from the entrance through the first gate. The three-map journey passed encounter counts, boss/portal gates, prompt acceptance and decline, Stage 01 completion, energy carryover and restart reset. Physics collision, ragdoll and single-reward regression checks also passed. Journey checks simulate defeats and do not establish manual combat balance.

Player-camera screenshots were reviewed for all three maps during the scenery pass. This caught and corrected overlapping bridge flicker and missing instanced-material usage flags. The astral map also has a soft opposing fill light to improve player visibility.

Before the latest area-damage and boss-protection update, the editor build, initial enemy checks and complete three-map journey with energy carryover/reset passed again, along with physics, energy and counter regressions. A rendered preview verified Commander phase 2, three reinforcements and the boss HUD. Expanded contact checks passed the new attacks, slam coverage and blocked rush. The standalone combat-rule test, editor build and runtime combat balance, enemy moveset, energy, physics, counter and three-map progression checks passed for the latest changes. See ENEMY MOVESET PLAYTEST.md for the current validation and visual notes.

Manual playtest priorities: traverse every branch and jump, check camera collision near walls, fight each group naturally, test lava escape and void recovery, then use the portals through Stage 01 completion.
