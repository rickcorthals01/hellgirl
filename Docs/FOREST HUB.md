# Forest camp hub

Unlocked after completing Level 1 (Goblin Queen and her remaining summons). Completed campaign levels now return here through their exit portal. Once unlocked, the normal game startup opens camp; the pause menu also offers Forest Camp. Explicit campaign/legacy launch options still work.

- Circular forest clearing, campfire with warm flickering light, seating/bedroll, merchant table, and dirt trail fading into a dark forest arch.
- Approach the campfire and press E / controller Y to heal and choose Rags or Goblin Queen. The Queen outfit is available after Level 1. Other outfit assets remain intact; the camp's outfit list stays limited to these two.
- Approach the road and press E / Y for unlocked level selection. Levels 1 and 2 are playable; later entries remain unavailable.
- Approach the friendly goblin and press E / Y to open the shop. This first pass displays the wallet and a coming-soon stock message; prices, upgrade effects and purchases are not implemented yet.
- Combat inputs are disabled in camp, and the merchant is a decorative friendly actor with no combat AI or health. Dodge/sprint remain available. Menus support mouse, keyboard and gamepad focus; Escape / B returns to camp.
- Departing camp starts a fresh level with full health and empty energy. Coins, unlocked levels and equipped outfits retain their existing persistence.

The previous battle arenas remain unchanged. The Succubus Hall and shop economy are future work.

Development preview: launch Entry?ForestHub=1. `work/run-hub-check.ps1` tests the three interactions and round-trip travel without altering progression; `-Preview` creates work/ForestHub.png. Test/preview flags simulate the hub unlock only for that process.
