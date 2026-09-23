# Right and left punch — first animation pass

Open AshenArena.uproject and press Play. Left-click once for the right punch. Click again near the end of recovery to queue the left punch. Further combo attacks still use the existing gameplay logic but have no new animation yet.

Both punches last 0.32 seconds. The contact pose is at 0.13 seconds, close to the existing damage time of 0.128 seconds. Playback follows the combat clock and returns to a shared held guard pose. This guard is static; walking, jumping, dodging and other attacks still await their own animation.

Editable Blender files and FBX exports are in Stage 01 Hands/Assets/Player Character - Hellgirl/Animations/Punches. Imported clips are in Content/HellgirlTest/Punches. Each Blender file includes anticipation, contact and recovery markers.

Checked both contact poses in Blender renders and compared all 33 samples of each imported clip against source joint positions in Unreal. The game C++ build passed. Interactive attack feel still needs playtesting. These are first-pass straight punches: finger/thumb shapes and extreme-bend surface deformation remain polish work.

A reference clip would help choose the next revision's style: compact boxing punches, a loose brawler style, or exaggerated action-game strikes. No reference was needed to make this initial pair.
