# Perfect dodge counter

Press the existing dodge button (Shift / controller B) during the bright flash on an enemy's striking hand to turn the dodge into an immediate counter. The placeholders currently fight with their hands; the flash component can be attached to a weapon socket when armed enemy assets arrive.

Initial tuning:

- Flash and counter window: final 0.3 seconds before attack contact (extended from 0.18 seconds after playtesting).
- Counter: 45 damage, strong backward launch with upward lift, and 1.2 seconds of knockdown.
- Costs the normal 30 dodge stamina and uses the existing cooldown and brief invulnerability.
- A nearby enemy must actually be attacking toward the player, within its hit range and height, with an unobstructed line between them.
- If several threats qualify, counter the one closest to contact. One button press counters one attacker.
- Stored riposte boosts the counter and is consumed on success, like other successful attacks.
- Early or out-of-range dodges remain normal dodges. The cancelled enemy swing cannot hit later.
- Uses the existing punch clip for the counter; a dedicated animation and polished weapon-flash effect remain future art work.

The Unreal target compiled. The unattended counter test covers early/late timing, facing, range, insufficient stamina, successful damage, attack interruption, and prevention of repeat counters. Visual readability and timing feel still need an in-game playtest.
