# Hellgirl — first unarmed combat pass

Latest update: use **CHARGE AND AIR PLAYTEST.md** for the new hold-heavy charge and four-step aerial combo. Those supersede the single-air-attack rule below. See **COINS PLAYTEST.md** for the wallet and coin drops.

This chat handles gameplay. The user is creating models and textures in a separate chat; keep using placeholders here until those assets are ready.

## Build and play

Close Unreal Editor, then double-click **Build and Open.cmd** in this folder. Press Play after the build succeeds and the editor reopens. Do a full rebuild for this change because character components and reflected functions changed.

The previous movement fix was built and the user reported it working. This new combat pass has passed engine-independent C++ move-selection tests and input consistency checks. Its full Unreal rebuild and gameplay test are still pending.

## Controls

| Action | Keyboard / mouse | Xbox controller |
| --- | --- | --- |
| Move | WASD | Left stick |
| Look | Mouse | Right stick |
| Normal | Left click | X |
| Heavy | Right click | Y |
| Dodge | Shift | B |
| Jump | Space | A |
| Restart | R | Not assigned |

Space now jumps. Shift now dodges. Watch the move name and HIT/MISS readout in the HUD. The first enemy wave starts after eight seconds so you can try the sequence before fighting.

## Try these sequences

1. Four normal attacks: right punch → left punch → turning kick → other-foot kick. The next normal returns to right punch.
2. One normal then heavy: headbutt.
3. Two normals then heavy: high-damage elbow.
4. Three normals then heavy: tackle damage, knockback, and knockdown.
5. Four normals then heavy: shoulder-throw damage and knockdown.
6. Dodge, then immediately normal: uppercut. Dodge then heavy: leg sweep with knockdown.
7. Jump then normal or heavy: air punch or air slam. Only one attack per airborne period is allowed in this prototype.

Press the next attack near the end of the current move; one input can be buffered during the last 0.22 seconds. A grounded combo stays available for 0.65 seconds after recovery. Heavy branches reset the normal sequence. Being hit interrupts attacks and resets the sequence. Dodge invulnerability lasts 0.25 seconds, costs 30 stamina, and has a 0.65-second cooldown. Post-dodge follow-ups are available for 0.4 seconds after the roll.

All timings and damage are provisional tuning values. The heavy-without-a-combo move, aerial moves, and guaranteed boosted uppercut damage are implementation choices for testing, not finalized design from the concept file.

## Current visual limits

The player and enemies are primitive bodies with visible head, hands, and feet. Procedural poses distinguish punches, kicks, dodges, and knockdowns. Tackle, leg-grab, and shoulder-throw branches currently apply combat effects and placeholder poses; they do not yet align and animate both characters through a real grapple. The elbow is a guaranteed high-damage branch; probabilistic critical-hit rules are not implemented.

The arena still has the earlier five-wave test loop. Apartment intro, Stage 1 boss/objective, stage saves, special meter/ability, sprint, weapon selection, keyboard rebinding menu, sounds, and final animation integration remain future work.

## Gameplay verification

Check movement and camera, keyboard and controller actions, all four normal steps, each heavy branch, combo timeout, interrupted attacks, post-dodge expiry, jumping and landing, hit/miss range, knockdown recovery, dodge stamina/invulnerability, and one R restart without repeated reloads. A passed move-selection test does not validate animation feel or in-engine behavior.
