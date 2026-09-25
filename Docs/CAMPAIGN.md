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

Balance update (2026-09-25): ordinary enemies have 35% less health and every non-boss wave spawns 50% more enemies (rounded), so a wave holds about the same total health spread over more bodies. Bosses keep their health; their summons count as ordinary. Both numbers live in Source/Hellgirl/Rules/EnemyTuning.h. Maps keep their authored wave sizes; AEnemySpawnPoint::WaveTotal() applies the scale.

Currency (2026-09-25): the currency is now **souls** instead of coins. Defeated enemies drop glowing blue soul wisps that drift to Hellgirl, with the same drop rules as before. The HUD, shop, merchant and save slots all say souls. Internally the code still calls them Coins (UHellgirlWallet::Coins, ACoinPickup), so existing saves keep their balance. Docs/History describes the older coin version.

Pacing (2026-09-25): the breaks between waves are a third of their authored length (`EnemyTuning::WaveBreakScale`). The forest's 3 s becomes about 1 s, and the castle, Imp arena and goblin stage go from 3–4 s to about 1–1.4 s. Each level's opening delay is unchanged. Enemies in a trickling wave arrive every 0.3 s instead of every 0.55 s (`EnemyTuning::SpawnInterval`).

## World I story flow (2026-09-25)

Built from the scripts in `Developer idea folder lol\Dialog and Story` (01 to 03.5). Stages 2 and 3 are scripts of steps taken in order: waves, conversations, soul portals, the Queen and the exit. The scripts are in `Source/Hellgirl/Levels/GoblinWaves.cpp`. A step is finished once its sites are cleared, or once its conversation or portal id is in `PlayedStory`, so a loaded save resumes where it left off.

- **Stage 1:** unchanged, but it now leaves through the new exit portal.
- **Camp after Stage 1:** "This place looks safe.. I'll set up camp here" plays on a black screen, then camp fades in.
- **Stage 2 (the goblin army):** Hellgirl starts in the walled middle of the ruins.
  1. "..." plays, then waves 1–3.
  2. A soul portal opens, with its help text. Continue closes it.
  3. Waves 4–6, then a second portal.
  4. The Queen: "Face the might of my entire brood…". Hellgirl answers "...!".
  5. Wave 7: two huge packs charge in from both ends of the map.
  6. The exit portal opens.
- **Camp after Stage 2:** "Endless mode has been unlocked for Goblins." appears in a box over camp.
- **Stage 3 (the Queen):** 14 waves across the castle's three sections.
  1. Waves 1–2, then "GOBLIIIINSSSSS!" and "Kill her!".
  2. Waves 3–4, then a portal. Continuing opens gate 1.
  3. Waves 5–6, then "Talk to me!".
  4. Waves 7–10, then "Subjects are not supposed to fight back!".
  5. A portal. After continuing, Hellgirl says "Subjects?".
  6. Waves 11–14, then a portal. Continuing opens gate 2, the boss room.
  7. The Queen speaks once she appears: "You've been unruly enough!".
  8. At 30% health: "You're … strong!?" and "I'll show you!". This unlocks **ultimates**: the energy bar fills and an on-screen tip names the ultimate key, taken from the input settings.
  9. At the end she begs (the full "first circle of Hel" conversation) and runs off. A narration box follows ("Hellgirl seems distracted…"), then "Huh, wait!", and the exit portal opens.
- **Camp after Stage 3:** "A goblin has followed Hellgirl to her camp." Talking to him the first time plays his lines and unlocks the shop, which then opens.

**One-time unlocks:** `Source/Hellgirl/Progress/CampaignProgress.h` holds these flags: `CampSetUp`, `Stage2Won`, `Stage3Won`, `EndlessGoblins`, `UltimatesUnlocked`, `GoblinFollowed`, `ShopUnlocked`. Unlocks follow the scripts strictly, whatever older progress says: endless needs a Stage 2 win, the goblin and his shop a Stage 3 win, ultimates the Queen's 30% moment.
- They are stored in `[HellgirlCampaign]` and copied into save slots. A new game clears them.
- Automated runs (`-Hellgirl…`) keep the flags in memory only. Every check except the story check has ultimates unlocked.
- Until they unlock, the ultimate key says "YOUR POWER IS STILL SEALED" and the controls hint leaves the ultimate out.

**Souls:**
- Souls picked up in a level are **carried**, and shown under your stocked total ("+N carried").
- They are banked when you **win** the level or **stock** them at a soul portal.
- **Falling loses every carried soul.** Leaving any other way (camp from the pause menu, restart) forfeits them too.
- A forest run carries them from room to room and banks them when the run is won. Camp pickups go straight to the bank.

**Portals:**
- **Soul portal:** Continue / Stock souls / five upgrade offers (see below). In endless mode there is also a Leave option.
- **Exit portal:** "Leave the map and go back to camp?" YES / NO.
- Walking in opens the menu. After closing it, E / Y reopens it.
- Both use the kit's rune gateway (`AWavePortal`): blue for the soul portal, purple for the exit.

**Endless (goblins):**
- Unlocked by beating Stage 2. It is on the World I level select, with your best wave.
- It uses the Stage 2 arena (`CampaignLevel=2?Endless=1`). Waves grow, the goblins get tougher every 4 waves, and every 5th wave is an army charging from both ends.
- A soul portal opens after every 3rd wave.
- Falling ends the run and loses the carried souls. Leaving at a portal banks them.
- The best wave is saved as `EndlessGoblinsBest`.

**Checks:**
- `GoblinStage` walks both scripts: wave order, portals and gates.
- `Story23` plays Stage 2, then Stage 3: every conversation in order, the 30% unlock and the Queen's escape.
- `Endless` runs ten waves, then a death.
- `-HellgirlStoryCheck -StoryCamp` at camp checks the three camp moments and the shop unlock.
- `-StoryShots` photographs every page, portal and menu.

**Waves always finish (2026-09-25 fix):**
- **Spawning:** a wave's spawn spiral is at most 650 units wide. A spot inside a rock, or behind a wall or closed gate from its site, is retried at other angles, then at the site itself, and skipped after that.
- **Stage 2 army:** its packs now start at X −2400 and 2300. At ±2850 they spawned past the gates, so the wave, and the exit portal after it, never finished.
- **Stragglers:** once a wave is down to its last four goblins and 12 s pass without a kill, goblins more than 900 units from Hellgirl are brought back near her. Goblins that fell through the floor are brought back too. This covers goblins knocked onto a wall top or stuck on a ledge.
- **`NaturalWaves` check:** runs Stage 2, Stage 3 and endless for real. Hellgirl stands still and only goblins that reach her die. It fails on a stall, or if a goblin appears beyond its wave's gate.

**Soul portal upgrades (2026-09-25):**
- Each blue portal rolls **5 random offers** from a pool of 8. They are paid with **carried** souls, so every portal is a choice: spend them on power now, stock them to keep them, or risk carrying them.
- Each offer can be bought once per portal. There is no limit on how many you buy, so saving up lets you buy several at once.
- **Prices:** set to about one portal's worth of souls. A Stage 2 portal drops about 40 souls and later portals 60–120. Each level you already own makes the next one 40% dearer.

| Upgrade | Effect per level | Base price |
|---|---|---|
| Fury | +15% damage | 50 |
| Iron Skin | −15% damage taken (at most −55%) | 45 |
| Vitality | +25 max health, healed | 35 |
| Swiftness | +10% move speed (at most +40%) | 30 |
| Soul Hunger | +25% energy from hits | 35 |
| Second Wind | heal half your health (spent at once) | 25 |
| Bloodthirst | heal 3 per kill | 45 |
| Greed | +1 soul from every drop | 40 |

- Upgrades last for the rest of the level or endless run. A new level starts without them.
- They are shown above the health bar and kept in save slots.
- **Code:** the rules and stats are in `Source/Hellgirl/Rules/PortalUpgrades.h`, buying is in `Levels/SoulUpgrades.cpp`, and the stats apply through `AArenaFighter::Upgrades`.
- The `Endless` check buys offers at each portal: it verifies the price, that each offer sells once, the stats and fresh offers.

**Goblin tuning (2026-09-25):** goblins hit harder and attack faster. Their dagger slash deals AttackDamage x1.2 (13 at difficulty 1, was 10, and more in later endless waves). The slash cycle is 1.9 s (was 2.55 s). Up to six goblins attack at once (was two), with attack starts at least 0.3 s apart (was 0.45 s). Other enemies are unchanged. The numbers are in `Rules/EnemyTuning.h`.
