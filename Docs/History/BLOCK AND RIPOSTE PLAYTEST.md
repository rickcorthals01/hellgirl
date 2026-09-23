# Block and riposte playtest

Implemented and compiled September 12, 2026. Standalone defense-rule checks passed; visual gameplay testing remains pending.

## Controls

- Hold Q / controller LB to block.
- Hold F / controller left-stick click to sprint at 1.5 times normal speed.
- Existing Ctrl slow walk remains available.

## Behavior

Block covers a 120-degree cone in front of the player and reduces incoming damage by 75%. Prevented damage fills the visible riposte meter, up to 100. Chip damage can still kill the player. Blocking resists knockback from the blocked hit.

The next attack that damages an enemy consumes the stored meter for up to double damage. A miss keeps the meter. All enemies hit by one attack receive the same bonus. Riposte resets on death or restart.

Holding block faces the camera direction and slows movement. Guard is inactive during attacks, dodges, heavy charging, stagger, or while airborne; it resumes when those actions finish if the button remains held. Pressing block cancels a pending heavy charge. Sprint does not boost guarding, attacks, or slow walking.

Enemies now wind up for approximately 0.75 seconds before contact, with an orange direction indicator and an ATTACK INCOMING label. Their placeholder attack pose pulls back before striking. The player has a cyan guard arc and a HUD state indicator; a dedicated skeletal guard animation remains future asset work.

## Try in Play mode

1. Approach a rift and face an enemy. Hold block through its orange wind-up. Confirm health loses less damage and riposte increases.
2. Let an enemy hit from behind. Confirm full damage and no additional riposte.
3. Swing out of range. Confirm the stored meter stays. Land the next attack and confirm it clears.
4. Try block around a dodge, jump, and charged attack. Confirm guard returns while the button remains held.
5. Compare ordinary movement with held sprint, then block while sprint is held. Confirm guarding stays slow.

Adjust damage reduction, meter gain, and enemy timing after assessing the feel in gameplay.
