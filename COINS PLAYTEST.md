# Coins and wallet

The controller's vertical look mapping has been reversed from the previous build, as requested. Mouse look is unchanged.

Each defeated enemy rolls once for a coin stack:

- 95% chance of 1, 2, or 3 coins, equally likely within this tier.
- 5% chance of 4, 5, or 6 coins, equally likely within this tier.

A placeholder coin pickup and a gold value label appear at the enemy's horizontal position, on the flat test arena floor. Walk within 150 units to collect after a short 0.35-second spawn delay. Living players can collect; enemies cannot. Coins can still be collected after arena victory. A stack credits its full value once and disappears.

The wallet appears at the upper right. Collected coins save automatically to the local HellgirlWallet_v1 slot and survive R restarts and editor/game restarts. Uncollected drops disappear with the current arena on restart. This is one local wallet; spending, multiple profiles, and cloud saves are not included.

If a save fails, the balance remains in memory across arena restarts, the HUD reports the failure, and saving is retried on the next pickup and orderly shutdown. An unreadable existing save is not overwritten; collection is blocked and the HUD reports it.

## Validation

The C++ drop-rule test passed all 300 combinations of tier and amount rolls. Exactly 15 combinations yield 4–6 coins, proving a 5% rare tier under uniform rolls; the other 285 yield 1–3. Full Unreal compilation and the following in-engine checks remain pending:

1. Close Unreal and run Build and Open.cmd, then press Play.
2. Check up/down on the controller's right stick.
3. Defeat enemies; verify a pickup appears with a value from 1–6.
4. Walk over one and confirm a single matching wallet increase.
5. Press R; the collected balance should remain.
6. Close/reopen the editor, press Play, and verify the saved balance.
7. Collect a final-wave drop after the victory message.

Rare drops are probabilistic: twenty kills do not guarantee one rare drop.
