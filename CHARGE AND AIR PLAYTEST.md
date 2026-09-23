# Charge and aerial combo pass

Based on the new Charge attack and Aerial attacks sections of Combat Design info.txt. Models and textures remain work for the user's separate chat; this implementation uses primitive poses.

## Try it

Close Unreal Editor and run **Build and Open.cmd**, then press Play.

- Tap right mouse / Y for the existing heavy move or heavy combo branch. Grounded heavy now activates on release to distinguish a tap from a hold.
- Hold right mouse / Y, then release: charged area strike. The HUD displays charge percentage and an orange ground circle previews range.
- Jump, then press normal four times: right punch → left punch → kick → crash kick. The fourth move descends and damages nearby enemies on landing.
- The first aerial punch automatically dashes toward a visible flying enemy in front within short range. The third hit can dash from medium range. The second punch continues from the current position.
- The finisher pulls the previously targeted flying enemy down when still nearby and unobstructed. Its ground-impact radius catches the target and surrounding enemies.
- Airborne heavy retains the provisional direct ground slam.

One floating test enemy is added from wave 2 onward. It uses a basic hover/pursuit behavior rather than a final enemy design. A pulled-down flying enemy stays grounded in this prototype.

## Provisional tuning

- Tap threshold: under 0.2 seconds of active charging. Charge reaches full power at 1.5 seconds and is capped there until released.
- Charged damage scales from 28 to 80, and radius from 190 to 460 Unreal units. Actual charged releases start above the tap threshold. The strike hits all eligible enemies within range, in every direction, with line-of-sight checks.
- Grounded heavy branches remain available on taps. A charged release uses the charged move and resets the combo.
- Normal input during charging is ignored. Dodging cancels charging when available; damage and releasing the cursor also cancel it.
- Short aerial dash range: 350 units; medium: 600. Both are editable on the fighter, along with maximum charge time.
- The first three aerial attacks briefly suspend falling, with a total 2.4-second suspension budget per jump. This allows the four inputs without unlimited hovering. Stop attacking and gravity resumes.
- Crash kick: 55 damage, 360-unit radius, and knockdown. Impact occurs on landing, once, rather than on an airborne animation timer. A three-second safety timeout cancels an unfinished dive without awarding impact damage.
- Enemy pull-down requires the existing flying target within 260 units and a clear line of sight. Dashes use character movement and world collision, not teleportation.

These timings, ranges, and damage values are starting choices; the source design does not prescribe numbers. This is a functional placeholder implementation, not finished animation choreography.

## Validation status

The standalone C++ combat-rule tests pass: existing grounded branches, four aerial stages in order, landing-impact move classification, monotonic charge damage/radius across 100 charge levels, and charge bounds.

Full Unreal compilation and playtesting for this update remain pending. After rebuilding, check:

1. Grounded tap-heavy branches still work after all four normal hits.
2. Short versus full charges show different radii and damage; multiple enemies can be hit together.
3. Taking damage or dodging ends charging without releasing an unwanted attack.
4. Four aerial normals execute in order; ground impact triggers once and hits surrounding enemies.
5. Against the wave-2 flyer, hit 1 dashes from short range and hit 3 from medium range. Walls should block targeting and movement.
6. Hit 4 pulls down a nearby flying target and lands before its area damage occurs.
7. Landing, interruption, and the hang timeout restore gravity. Movement works normally on the ground afterward.
8. Coin collection and the saved wallet continue working after airborne kills.
