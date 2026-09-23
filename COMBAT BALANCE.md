# Combat balance and dodge cancellation — September 12, 2026

| Move | Previous damage | New damage |
| --- | ---: | ---: |
| Ground punches (each) | 12 | 18 |
| Turning kick | 17 | 24 |
| Follow-up kick | 20 | 28 |
| Heavy punch | 25 | 34 |
| Headbutt | 28 | 38 |
| Elbow | 40 | 52 |
| Tackle | 32 | 43 |
| Shoulder throw | 45 | 59 |
| Dodge uppercut | 26 | 35 |
| Leg sweep | 23 | 31 |
| Air punches (each) | 12 | 18 |
| Air kick | 18 | 25 |
| Air combo crash finisher | 28 | 20 |
| Air heavy slam | 35 | 22 |
| Charged strike (minimum to full charge) | 28–80 | 38–100 |

Dodge now immediately interrupts attacks on the ground or in the air. It clears the pending hit, buffered combo input, heavy charge, aerial dash/hang, and any pending slam landing damage. It restores normal gravity. Cancelling a slam also reduces its forced downward speed so the dodge can escape the dive. A cancelled aerial finisher remains spent until landing.

Damage already dealt is retained. Cancelling before contact preserves riposte. Failed dodge requests (insufficient stamina, cooldown, knockdown, or death) do not cancel the attack. Dodge still costs 30 stamina and has its existing cooldown. Attack input during the end of a dodge can still buffer a new follow-up.

The move-selection checks passed and the Unreal project compiled. The unattended combat check verifies cancellation state, stamina eligibility, preserved riposte, and no damage from a cancelled swing or landing impact. Manual combat-feel playtesting is still needed.
