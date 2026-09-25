# Hellgirl combat specification

Recorded September 10, 2026 from `Combat Design info.txt` and `Stage 01 Hands/Hands Weapon info.txt` in the project root. These are design requirements, not a claim that the current starter implements them.

## Inputs

| Action | Xbox controller | Keyboard / mouse |
| --- | --- | --- |
| Normal attack | X | Left click |
| Heavy attack | Y | Right click |
| Dodge roll | B | Shift |
| Jump | A | Spacebar |
| Special move | Right stick click | Not specified |
| Sprint | Hold left shoulder or left stick click | Not specified |
| Weapon selection | D-pad: up fists, right sword | 1 fists, 2 sword |

Expose keyboard/mouse bindings in a settings menu; controller bindings remain fixed. The weapon selector has two entries and opens when a D-pad direction is pressed. The gun and car were removed from the design on September 23. Selection confirmation and whether time slows while selecting are not specified. Sprint behavior for a stick click (hold versus toggle) also remains unspecified.

## Combat rules

Latest source update: the newly supplied charge and aerial descriptions are implemented provisionally in the charge/aerial pass. See **CHARGE AND AIR PLAYTEST.md** for controls, tuning, and validation. Hold/release heavy now scales area and damage; aerial normal attacks now have four steps with flying-target dashes and a landing finisher. This supersedes the earlier one-air-attack implementation.

- Quick normal attacks deal low damage; slower heavy attacks deal high damage.
- Combo attacks build toward special ability use. Meter gain, activation cost, and special move effects remain to be defined.
- Dodge rolls grant a brief invulnerability window.
- Normal and heavy attacks each have an airborne version after jumping; their exact moves are not specified.
- Stage 1 starts after Hellgirl lands through the TV hell portal, confused, without armor or weapons. She fights with her fists. Use the renamed Stage 01 Hands asset folder for references.

## Grounded fist combo

| Normal attack count | Move | Heavy follow-up |
| --- | --- | --- |
| 1 | Right-hand forward punch | Headbutt |
| 2 | Left-hand punch | Elbow strike from the left punch; critical damage |
| 3 | Turning kick that builds momentum | Bend down, spring forward, and tackle; knockback and knockdown |
| 4 | Kick with the other foot using that momentum; return to initial stance | Mount enemy neck/shoulders and throw the enemy down |

Heavy can branch after any normal hit. The notes describe a repeated four-hit normal sequence. Timing windows, buffering, interruptions, and the behavior of a heavy attack without a preceding normal attack remain to be decided.

## Attacks immediately after a dodge roll

| Input | Move |
| --- | --- |
| Normal attack | Upward sucker punch; can critically hit |
| Heavy attack | Grab the demon's legs and knock it down |

The follow-up window duration is unspecified. Enemy size restrictions and animation alignment for grabs, tackles, and throws also need design and implementation.

## Later weapons

Sword combat is explicitly WIP in the source. Keep their moves open rather than applying the fist move list to them. Stage-based unlocks and permanent upgrades follow HELLGIRL DESIGN.md.

## Implementation implications

Update September 11: the first implementation pass adds the unarmed sequence and heavy branch selection, jump and aerial variants, post-dodge variants, buffered input, interruption/knockdown effects, and basic controller mappings. See **FISTS PLAYTEST.md** for its exact scope and validation status. The paragraph below describes the pre-change scaffold. Special meter, sprint, weapon selection, rebinding, and coordinated grab animations remain unimplemented.

The existing generic scaffold lacks heavy attacks, jumping bindings, controller support, rebinding, the four-hit fist sequence, contextual attacks, a special meter, weapon selection, and grab/knockdown animations. Its Space-to-dodge mapping must change to Shift-to-dodge and Space-to-jump when these controls are implemented.

A combat state system should distinguish grounded normal combo position, heavy branch, dodge and post-dodge window, aerial state, hit reaction, knockdown, and death. Animation events should eventually determine contact timing. Coordinated throws require suitable attacker and victim animations; a capsule knockback effect alone does not implement the described move.

Suggested build order: first obtain a successful baseline compilation; then implement the input and unarmed combat states with labeled placeholders; then add animations, move-specific hit detection, special meter, and the keyboard settings menu. This is an implementation proposal, not additional confirmed game design.

## Style combo meter (2026-09-25)

Landing hits fills a combo meter. Each full bar adds 0.1 to a damage multiplier, from **1.1x** up to **2.0x**, and every hit you land gets that bonus.
- **Variety counts:** moves are grouped into families: light punches, heavy kicks, air lights, air slams and crash kicks, dodge counters, the charged strike, and sword. A family you haven't used in your last three hits gives a full fill. Going back to one used a moment ago gives 75%. Repeating the same family gives less each time: ×0.55 per repeat.
- **Numbers:** a varied fight reaches 2.0x in about 25 hits. Mashing one button stays under 1.1x, and the HUD shows "VARY YOUR MOVES".
- **Losing it:** after 3 s without landing a hit, the meter drains, and the HUD warns "keep hitting" as the 3 s run out. Taking a hit costs two tiers.
- **Details:** each attack counts once, however many enemies it hits. The bonus uses the meter as it was before the hit.

All numbers are in `Source/Hellgirl/Rules/ComboRules.h`. `-HellgirlComboCheck` tests the rules and a 1.5x punch. The bonus stays off in other automated checks, so they keep measuring exact damage. The HUD shows the multiplier under the minimap: parchment at low tiers, then gold and ember, then red. It pops when it goes up.

**Dodge slam (2026-09-25):** dodge + heavy is an area slam.
- **Reach:** 400. **Damage:** 26 at the centre, softer toward the edge.
- It launches everyone it hits away from Hellgirl.
- It costs a full energy tube (`HellgirlEnergy::DodgeSlamCost`); other slams cost half a tube.
