# Hellgirl combat energy playtest

Energy powers charged strikes and aerial slams. The HP and energy bars sit at the bottom left, with charge/slam readiness beneath them. Dodge stamina and riposte remain in the upper combat panel. The September 15 balance pass reduces area damage while retaining energy costs and momentum.

## Building and spending energy

| Action | Energy change |
|---|---:|
| Successful normal ground attack, dodge uppercut, or non-area aerial attack | +12 |
| Successful uncharged heavy punch or heavy combo branch | +18 |
| Charged strike | −40 |
| Aerial slam or aerial crash-kick finisher | −35 |

Energy starts at **0** and is capped at **100**. A fresh start or map restart resets it; moving forward through a portal carries the current amount into the next map.

A basic attack earns energy only when it actually damages an enemy. Each swing earns its amount once, even if several enemies are hit. Misses earn nothing. Charged strikes, slams and crash-kick finishers do not generate energy, including when they hit a group.

Special attacks spend energy when the attack is accepted and begins. Holding the charge button or queuing an input does not spend energy early. Once the move starts, a miss or dodge cancellation does not refund its cost.

An attempted heavy aerial slam requires at least 35 energy. With less than 35 energy, the fourth normal aerial attack instead becomes a free basic aerial kick that ends the combo; it does not perform the crash kick or its area impact. A successful fallback kick earns the normal 12 energy.

Charged strikes now deal 12–28 base damage, aerial slams 16 and crash kicks 14. Area damage is full within the inner quarter of the radius and falls to 55% at the edge. Collision damage from all bodies launched by one area attack is capped at 6 per victim, preserving launch force and chain motion. Maximum riposte can double direct damage, but one fully charged strike and its collisions total at most 62 damage against a fresh 67-HP Imp. Normal and uncharged heavy damage, energy gains and the 40/35 costs are unchanged.

Bosses still take damage while attacking, so a successful basic hit can earn energy, but their active attacks and summons cannot be interrupted. A timed dodge against an attacking boss remains a dodge; regular enemies can still be countered.

## Try these checks

1. Start or restart a map and confirm the energy meter is empty. Land basic attacks and watch it increase; swing at empty space and confirm that it stays unchanged.
2. Hit several enemies with one normal or uncharged heavy attack. The energy gain should be 12 or 18 for the swing, rather than multiplied by the number of enemies.
3. Build at least 40 energy. Hold **right mouse / Y** to charge, then release. Energy should be spent when the charged attack starts. Charge again without enough energy and confirm that the special does not execute.
4. Begin a charged strike and immediately dodge with **Shift / B**, or aim the strike away from every enemy. Its spent energy should remain spent.
5. Jump with **Space / A** and attempt a heavy aerial slam at less than 35 energy, then try again with enough energy. The slam should only begin when its cost can be paid.
6. At zero energy, miss the first three normal aerial attacks and press normal again. The fourth input should produce a basic kick with no slam impact. If all three earlier hits connect, they earn 36 energy, enough for the fourth input to spend 35 on the crash-kick finisher.
7. Watch the meter after a special hits several enemies: the special should not replenish itself. Clear an encounter and move through a portal to check that remaining energy carries forward; restart to check that it resets.

Latest validation, September 15: the standalone combat-rule test passed the new area-damage tuning. The editor build and runtime checks passed for the shared collision limit, boss attack protection, enemy movesets, energy, physics, counters and all three maps.

Earlier verification, September 14, 2026: the Unreal Engine 5.8.2 editor build succeeded. Runtime energy checks passed for real hit gains, one reward per swing, maximum capacity, charged and aerial costs, rejected inputs, buffering, misses, cancellation, aerial fallback, counters, collision damage, recovery and enemy independence. The complete three-map journey also verified energy carryover and restart reset. Movement and dodge-cancellation regression checks passed.

The movement test fixture deliberately begins each isolated physics scenario with full energy, so its charge-distance and collision checks remain independent of the energy-earning loop. The meter was rendered and inspected in game at 1280×720. The gains and costs are initial balance values for hands-on playtesting.
