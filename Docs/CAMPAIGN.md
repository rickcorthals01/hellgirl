# Campaign — current implementation

| Level | Enemies | Boss | State |
|---|---|---|---|
| 1 | Goblins | Goblin Queen (female goblin model) | Playable castle placeholder |
| 2 | Ground and flying imps | Imp Commander | Playable castle placeholder |
| 3 | Succubi | Succubus Queen | Planned |
| 4 | Ghosts | Ghost King | Planned |
| 5 | Rats | Rat Queen | Planned |
| 6 | Frogs | Frog King | Planned |
| 7 | Apostles | Lucifer | Planned |
| Special finale | Lucifer | Lucifer, Devil Form | Planned |

Level 1 uses one opening wave, two further waves, then the boss to match the new story. Level 2 keeps two waves, three waves, then its boss. Both use the castle's three gated sections. Level 1 has only ground goblins; Level 2 mixes ground and flying imps. Defeating each level unlocks the next. Completed levels return to the forest camp. The forest road opens unlocked level selection. Level 3 remains unavailable; the old lava/astral maps are excluded from campaign progression.

The previous lava (Gulps/slurpers) and astral (succubi) layouts are preserved in source and accessible for development using Entry?StageMap=2 and Entry?StageMap=3. Campaign levels use Entry?StageMap=1?CampaignLevel=1 or 2. Layout identity and campaign level are separate.

## Boss mechanics

Goblin Queen follows the new Levels, Enemies, Bosses design: melee initially; at 70% she disappears and summons two packs, returning after either pack is defeated; at 30% she summons another pack and shadow claws travel 50% faster; at 5% she disappears until ten goblins die, then returns at 50% HP. Packs continue every four seconds thereafter. Initial tuning: three goblins per pack, at most eighteen live summons; overflow waits until space is available. Thresholds cannot be skipped by a huge hit. Shadow projectiles travel toward the player's position at release and collide with terrain; dodge invulnerability avoids their damage.

Commander starts with three protective imps. While any remain, all incoming damage is reduced by 90%. His moves are wind-up rush, telegraphed area slam, and jump-slam with a 30-second cooldown and a short landing warning. Jump-slam replenishes protective imps up to three. Boss summons must also die before the exit opens. Future non-boss reuse has neither summons nor shielding and a longer jump warning.

Goblins run into melee for a slow dagger slash. Imps retain their faster mobile ground/flying moves. All non-boss enemies have a 5% chance to drop a health pickup restoring half maximum HP, capped at maximum. The health pickup and boss effects currently use placeholder visuals. Native goblin models/textures and supplied run animations are installed; dedicated attack animations remain future work.

## Deferred ideas

The first forest hub is implemented (see FOREST HUB.md), including the campfire outfit menu, level road and merchant interaction. The shop stock/economy and later Succubus Hall remain future work. Level 3's elaborate encounter and subsequent campaign entries are recorded, not made playable yet.

Balance update: encounter bosses have 900 HP (previously 450). Phase thresholds remain percentages.
