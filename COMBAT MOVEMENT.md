# Combat movement — September 14, 2026

Charged strikes now cost 40 energy, and aerial slams/crash finishers cost 35. Basic hits build energy; the movement and damage of affordable moves remain unchanged. See ENERGY PLAYTEST.md.

Attacks select a nearby, visible enemy in front of the camera, favoring the current opponent to avoid switching between enemies during a combo. The camera smoothly pans horizontally toward that opponent for up to three seconds after an attack. Dead or distant targets are released. Mouse/right-stick camera input temporarily overrides automatic tracking, and pitch/zoom stay under player control.

Ground right punch, left punch, and heavy punch dash toward the selected opponent: up to 320 units over 0.12 seconds, stopping approximately 130 units away. Punches without a valid target do not dash. Other ground strikes face the selected target but do not gain this punch dash.

A charged strike now produces a longer burst on release: 0.28–0.38 seconds at 1,300–1,800 units per second from rest, depending on charge. Incoming momentum can raise that speed to 2,000 units per second. This gives roughly 364–684 units of unobstructed burst travel from rest, or up to 760 with momentum, followed by a short coast. Direction is sampled from camera yaw when the move starts, never aimed at the combat target. Automatic camera tracking pauses while holding charge and during the charged strike.

Jump with **Space / controller A**, then dodge with **Shift / B** while airborne to leap farther. The first airborne dodge adds horizontal momentum while preserving vertical velocity; later dodges in the same jump cannot build extra speed or height. Landing restores the next boost. Initial dodge speed is normally 1,450–1,800 units per second, with limited steering in the air and a short landing coast.

Character movement handles collision sweeps, sliding and floor transitions. Walls stop the burst without repeatedly injecting velocity. Ground punches stop near their target and lose their targeted burst when leaving the ground; charges deliberately retain momentum and can carry Hellgirl off ledges. Dodge cancels an attack dash, and incoming stagger clears its momentum state. Strong attacks use approach speed to increase enemy launch strength; direct attack damage is unchanged by momentum.

Automated movement checks passed for range, ledge travel, finite airborne boosting, collision handling and travel distance across frame rates. Dodge cancellation and counter regressions also passed. Actual lunging-punch damage passed at 30, 60 and 120 fps and during a 200 ms frame delay; walls prevent damage. The timing fix waits for the final approach movement before resolving contact.

Manual playtest: punch toward nearby enemies, circle them between attacks, aim away and charge, jump then dodge across a gap, dodge midway through a punch, and test walls and stepping-stone edges. Camera tracking remains horizontal, with pitch and zoom under player control. Balance and visual feel still need hands-on feedback; see PHYSICS PLAYTEST.md for collision and ragdoll checks.
