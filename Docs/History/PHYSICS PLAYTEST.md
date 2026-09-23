# Hellgirl physics playtest — 15 September 2026

Implemented from the ideas folder's **Physics systems.txt**: Hellgirl carries momentum through dodges, leaps and charged attacks; strong attacks launch enemies into scenery or each other; defeated enemies become physical corpses.

Charged strikes require 40 energy, and slams/crash finishers require 35. Build energy by landing basic attacks first. The latest balance pass lowers their direct damage and caps the collision damage caused by each area attack while preserving launch forces. See ENERGY PLAYTEST.md for the combat loop.

## Try these first

1. **Leap:** move, jump with **Space / A**, then dodge with **Shift / B** while airborne. The dodge carries you farther horizontally; it does not add upward flight. Only the first dodge in each jump adds speed. Landing resets that allowance.
2. **Charge:** hold **right mouse / Y**, aim with the camera, then release. Hellgirl surges along the camera's horizontal facing direction and carries momentum after the burst. Charges can carry her over ledges. Ground punches still close the gap toward their combat target.
3. **Launch:** land a heavy punch, tackle, throw, charged strike or other knockdown move. Approach with momentum to increase launch strength. A successful timed dodge counter also launches a regular enemy. Bosses resist stagger, knockdown, launch and counters throughout their active attacks and summons, while still taking damage; idle bosses can stagger.
4. **Collision:** line up a launched enemy with another enemy, a wall or a large rock. A hard ordinary impact displays **IMPACT +damage**; an area-attack impact displays **IMPACT** because its shared damage allowance may already be spent. An enemy collision can damage both enemies and send the next enemy flying. One area attack must not repeatedly add damage to the same victim beyond its 6-point allowance, even across several launched bodies and walls.
5. **Death and pause:** defeat ground/flying Imps, Gulps and a placeholder boss. Their bodies should fall, tumble and settle while coins and encounter credit appear once. Pause with **P / Start** during a fall, then choose **PLAY** to resume.

## Initial tuning

| Behaviour | Starting values |
|---|---|
| Dodge/leap | Initial horizontal burst normally 14.5–18 m/s; existing vertical velocity is preserved. Limited air steering and a short landing coast. |
| Charged travel | 0.28–0.38 second burst; 13–18 m/s from rest depending on charge, up to 20 m/s with incoming momentum. |
| Strong attack launches | 6.5–16.5 m/s horizontally; momentum contributes to launch strength, without increasing the move's direct damage. |
| Perfect counter | 15 m/s backward launch with upward lift against regular enemies; the existing 0.3 second counter window remains. Timed dodges do not counter an attacking boss. |
| Impact damage | Closing speed must reach 4.5 m/s. Ordinary launch damage is clamped to **8–35**, rising with impact speed. One player area attack instead allows at most **6 total collision damage per victim** across its initial and secondary launches. Floor landings and contacts below the speed threshold do not count. |
| Collision chains | At most two additional launch transfers. Each launch hits a particular enemy once and counts at most one scenery impact. |
| Corpse lifetime | **12 seconds of unpaused game time**, or until leaving/restarting the map. |

Living enemies use controlled movement and knockdown reactions; full ragdoll simulation begins only at death. Existing skeletal PhysicsAssets drive Imps, FlyingImps and Gulps. Models without usable physics fall and tumble as one rigid capsule carrying their visible parts. Corpses collide with scenery, ignore living characters and cameras, and do not deal further collision damage.

Charged strikes deal 12–28 base damage, aerial slams 16 and crash kicks 14. Direct area damage is full within the inner quarter of the radius and falls to 55% at the edge. Radius, launch momentum and basic attack damage remain unchanged. At maximum riposte, a fully charged strike plus all of its collision damage is bounded by 28 × 2 + 6 = 62, below a fresh Imp's 67 HP. The same collision allowance follows a captured FlyingImp's downward launch before a slam lands. A later, separate area attack gets a fresh allowance.

Boss protection covers direct hits, physics impacts and timed dodge counters for the full active action, including summon recovery. Lethal damage and a reset can still end it. Regular enemy launch and interruption behavior is unchanged.

Death rewards are guarded against repetition, including deaths caused by collision damage. Corpses do not delay encounter completion. Pausing freezes normal physics and the cleanup timer.

Latest validation: the standalone combat-rule test passed the new damage tuning. The editor build and runtime checks passed for the shared area-impact limit, boss attack protection, enemy movesets, energy, physics, counters and all three maps.

Earlier physics verification: the build and runtime collision/corpse checks passed at **30 and 120 fps**, following the **60 fps** baseline. Checks covered actual wall impacts, enemy chains, speed thresholds, recovery, ground Imp/FlyingImp/Gulp ragdolls, the rigid placeholder fallback, and exactly-once rewards after lethal wall impacts. Pause/resume physics checks, the complete three-map journey, dodge cancellation, counters, and all four animation tests also passed; the animation report contained zero errors.

Movement checks passed for charge/leap range, ledges, the limited airborne boost, and travel distance across frame rates. Actual lunging-punch damage passed at 30, 60 and 120 fps and during a 200 ms frame delay; walls prevent damage. The timing fix waits for the final approach movement before resolving contact. Manual balance, leap distances across the three maps, and the visual feel of ragdolls still need hands-on testing. These values are a starting point for that feedback.
