# Neutral idle, walk and run

## Walk direction and jump update — September 12

Corrected the walk and run swing phases: a planted foot now moves backward relative to the body, then lifts while returning forward. Both cycles have matching start/end joint positions. The running cycle already exists and is selected above the walking-speed threshold; holding F uses the game's existing sprint behavior and speeds up playback.

Space now selects a jump pose sequence while airborne, followed by a short landing recovery when floor contact is detected. Jump height and collision remain controlled by character movement. This is a first animation pass; transitions and foot sliding still need interactive refinement.

All four imported clips passed comparisons against their Blender source, including 61 jump samples. The C++ rebuild passed. Updated sources are under Developer idea folder lol/Stage 01 Hands/Assets/Player Character - Hellgirl/Animations/Locomotion.

The new default idle lowers her arms. Punches temporarily use the combat guard and return to locomotion when the attack ends.

- WASD: run at the existing normal movement speed.
- Hold Left Ctrl while moving: walk at 40% speed.
- Controller: movement speed selects walk or run.
- Stand still: relaxed neutral idle with a subtle body movement.

Walking and running use looping in-place clips with speed-adjusted playback. The cycle phase is retained across speed changes. This initial integration switches clips directly; blended transitions, foot locking on slopes, jumping and dodging animations remain future work.

Files: Content/HellgirlTest/Locomotion. Editable Blender sources and FBX exports: Stage 01 Hands/Assets/Player Character - Hellgirl/Animations/Locomotion.

All 61 idle samples, 61 walk samples and 37 run samples passed joint-position comparisons in Unreal, within 0.01 mm of the Blender source. Preview poses were visually inspected. Interactive movement and transitions still need playtesting.

The game rebuild passed after the active Unreal session was closed.
