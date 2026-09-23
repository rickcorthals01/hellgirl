# Camera zoom

The camera starts at 420 Unreal units from its anchor, reduced from 650. Its existing framing, field of view, and spring-arm collision remain in place.

- Mouse wheel up: zoom in, 50 units per notch.
- Mouse wheel down: zoom out.
- Hold RT: zoom in continuously.
- Hold LT: zoom out continuously.
- Both triggers equally pressed: cancel each other out.

Zoom smoothly approaches the selected distance, bounded between 200 and 850 units. Controller zoom uses elapsed time and trigger pressure. Zoom pauses while the mouse cursor is released. Restart restores the default distance.

In-game check: try both input methods, hold each trigger to its limit, then approach a wall and move away to check camera collision and recovery. Visual playtesting remains pending.
