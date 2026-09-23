# Castle arena loop — September 18, 2026

The castle is now a three-section arena using the existing combat, scenery and models:
- Section 1: two waves (4 ground Imps; then 5 ground Imps and 1 flyer).
- Section 2: three waves (5, 7 and 8 enemies, including 1, 2 and 3 flyers).
- Section 3: Imp Commander, retaining his phase-two summons.

The existing castle walls mark section boundaries. Purple gates and full-width collision barriers prevent skipping locked sections. Clearing the section opens its gate permanently; the next section starts only after the player enters it. A three-second interval separates waves. The boss and any remaining summoned Imps must be defeated before the exit portal opens. Restart resets the arena encounters and gates.

This first conversion applies to the castle. Lava and astral maps retain their earlier encounters for later conversion. No hub, shop or upgrade system was added in this pass.

Validation: Unreal editor build passed. The runtime three-map journey passed, including castle wave ordering, early-activation prevention, gate states, boss/exit gating, portal accept/decline and energy carryover/restart reset.
