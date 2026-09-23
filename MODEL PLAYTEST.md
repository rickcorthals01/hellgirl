# Static Hellgirl model test

Open AshenArena.uproject and press Play. The player now uses the purchased Hellgirl mesh with skin, brown hair, eyes, and material masks, held in its neutral reference pose. Move and turn with the existing controls to judge scale and camera framing. She will slide while moving because no animation is assigned. The existing combat and jump mechanics still run, but the model does not perform their movements.

The player mesh is placed at the capsule's feet and rotated to face the movement direction. Enemies retain their placeholder shapes. Collision and movement still use the existing character capsule.

Build and headless game startup passed on 12 September 2026. Rendered appearance and foot placement still need the interactive playtest. Skin normals and specialized eye overlay shaders remain simplified for this first integration.

Model assets: Content/HellgirlTest. The pose-test animation is present as a reference asset but is not assigned to the player.

Planned animations, not yet created: right punch, left punch, right kick, left kick, dodge, tackle, jump, airpunch1, airpunch2, airkick1, airkick2, flyingdownslam.
