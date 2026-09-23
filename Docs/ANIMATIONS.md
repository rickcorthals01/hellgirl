# Combat animations

Hellgirl's attack, dodge, hit and death clips come from Mixamo and are fitted onto each outfit by `Tools/Animations`.

## Adding or replacing a clip

1. On Mixamo, use the same Meshy Hellgirl model as the existing packs (its 24-bone skeleton matches the outfits). Download **FBX Binary, Without Skin, 30 fps, In Place** when offered.
2. Save it under `Desktop\Hellgirl Game\Animations Mixamo\` with the file name listed in `Tools/Animations/clips.json` (most go in `Hellgirl Combat\`), or point `clips.json` at the file you downloaded.
3. Close Unreal Editor and run, from the project folder:
   ```
   powershell -ExecutionPolicy Bypass -File Tools\Animations\build.ps1
   ```
   Add `-Only RightPunch,LeftPunch` to rebuild just those clips.
4. Look at the result with the attack preview (needs a GPU; a game window opens for about 30 seconds):
   ```
   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" Hellgirl.uproject /Engine/Maps/Entry?ForestHub=1 -game -windowed -ResX=900 -ResY=900 -HellgirlAttackPreview
   ```
   Screenshots of every attack at wind-up, contact and follow-through land in `Saved/Screenshots/Attacks`.

## What the pipeline does

- **Fitting (Blender, `prepare_clips.py`).** Each bone receives the source bone's rotation relative to its rest pose, so the motion survives small rest-pose differences between models. Hip motion is scaled to each outfit's hip height. Net horizontal travel is removed, since the game moves the character itself during dashes; body sway within the move stays. A fit error above 1° stops the build.
- **Mirroring.** A left-side clip with `mirror_of` is generated from its right-side partner when its own file is missing.
- **Contact alignment.** For each attack, the frame where the `strike` limb reaches farthest from the hips is taken as the moment the hit connects (or set `contact` to a source frame). `build.ps1` writes these fractions to `[HellgirlAnimationContact]` in `Config/DefaultGame.ini`, and the game time-warps each clip so that frame lands exactly on the attack's damage moment.
- **Import (Unreal, `import_clips.py`).** Clips land in `/Game/Hellgirl/Outfits/<Outfit>/Animations/<Clip>` on each outfit's own skeleton. Intermediate FBX files go to `Animation Testing\CombatClips` (outside the repo).

The game loads whatever clips exist for the equipped outfit; anything missing falls back to the neutral pose. The Frog outfit has a different rig (28 bones) and is not covered yet.

## Clip names

| Clip | Used for |
|---|---|
| RightPunch, LeftPunch, DoubleJab | Light fist chain (right, left, right, double jab) |
| RightKick, LeftKick, LegSweep | Heavy chain (right, left, right, sweep) |
| Headbutt, DodgeSlam | Light / heavy right after a dodge (also the perfect counter) |
| Charge, ChargedStrike | Holding and releasing heavy |
| AirPunch, AirLeftPunch, AirKick, AirCrashKick, AirSlam | Aerial chain and air slam |
| SwordSlash, SwordBackslash, SwordThrust, SwordSpin | Sword light chain |
| Dodge, Hit, Knockdown, Death | Reactions |

Current sources: the sword slash, backslash and spin use the Great Sword pack. Everything else waits for downloads into `Hellgirl Combat\`.

The sword is still a placeholder box, now held in the right hand (`SwordGripOffset` / `SwordGripRotation` on the fighter).
