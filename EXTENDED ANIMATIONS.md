# Hellgirl extended animation set

Nineteen first-pass clips fill the remaining player combat states. Existing punches, ground kicks, idle, walk, run and jump are retained.

- Heavy moves: HeavyPunch, Headbutt, Elbow, Tackle, ShoulderThrow, ChargedStrike.
- Dodge and follow-ups: Dodge, DodgeUppercut, LegSweep.
- Aerial combo: AirPunch (airpunch1), AirLeftPunch (airpunch2), AirKick (airkick1), AirCrashKick (airkick2).
- Flying down slam: AirSlam.
- Support: Block, Charge, Hit, Knockdown, Death.

Clips follow the existing inputs and combo branches in FistCombatRules.h. Every player attack has its own animation. Strike playback follows combat progress; aerial finishers hold before contact until a real landing, and the immediate perfect counter starts at the uppercut contact pose. Dodge and knockdown take priority over strikes. Death holds its final pose and continues updating after health reaches zero.

Game clips: Content/HellgirlTest/Extended. Editable Blender scenes, FBX exports and contact previews: Developer idea folder lol/Stage 01 Hands/Assets/Player Character - Hellgirl/Animations/Extended. manifest.json stores duration/contact fractions; Unreal_checks.json stores per-frame import comparisons. Creation/import scripts: Animation Testing/create_remaining_moves.py and import_remaining_moves.py.

This is a first combat animation pass. Dodge is a low evasive duck matching the existing quick dash; shoulder throw animates Hellgirl and retains the existing knockback rather than adding a synchronized victim/grapple animation. Knockdown/death use a collapse pose, not ragdoll simulation. The short rag wrap can fold sharply during large leg lifts; cloth motion and transitions still need interactive visual polish.

Verification: editor build; every imported frame compared at six key joints; Hellgirl.Animation.PlayerMovePlayback checks all attack selections, time mapping, slam landing hold/release, dodge, charge, hit and death. Runtime encounter verification is recorded in ExtendedAnimationRuntime.log.
