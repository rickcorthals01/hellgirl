# Combat animations

Hellgirl's attack, dodge, hit and death clips come from Mixamo and are fitted onto each outfit by `Tools/Animations`.

## Adding or replacing a clip

1. On Mixamo, use the same Meshy Hellgirl model as the existing packs (its 24-bone skeleton matches the outfits). Download **FBX Binary, Without Skin, 30 fps, In Place** when offered.
2. Save it under `Desktop\Hellgirl Game\Animations Mixamo\` with the file name listed in `Tools/Animations/clips.json`, or point `clips.json` at the file you downloaded.
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
| Idle, Walk, Run | Standing, walking and running for every outfit except Frog (Mixamo Standing Idle and the Female Locomotion Pack; the Meshy walks kept tiptoe feet) |

Current sources: everything except SwordThrust has a clip; the sword slash, backslash and spin use the Great Sword pack. Air moves use `lift: 0` (the jump physics already lifts her), DodgeSlam keeps a third of its leap. Knockdown is Mixamo "Knocked Down" up to the moment she lies on her back, a short hold, then "Kip Up" at double speed: the game's knockdowns last only about a second, and the whole clip is fitted into that time. ("Getting Up" doesn't fit: it starts face-down with her head the other way round.) Hit ("Hit To Body") is fitted into the 0.3 s hit window.

The sword is still a placeholder box, now held in the right hand (`SwordGripOffset` / `SwordGripRotation` on the fighter).

## Tiptoe outfits

Meshy generated some outfits standing on tiptoe (heel 6–9 cm above the floor at rest), so every animation looked like she was on her toes. `Tools/Animations/flatten_feet.ps1` fixes an outfit: Blender rotates the feet flat at the ankle, bakes that into the mesh as the new rest pose and lowers the body onto the floor (5–9 cm shorter); Unreal re-imports the mesh and updates its skeleton; then all its clips are rebuilt on the new rest pose. Its own neutral pose is re-fitted from the original. Applied to Goblin Queen, Rat and Ghost. The originals are kept in `Animation Testing\ExtraSkins\<Outfit>\tiptoe`. Rags (2 cm) was left as is.

## Fists

The outfits have no finger bones, so the hands can't be posed by animations. `Tools/Animations/fist_hands.ps1` bakes a loose fist into each model instead. Blender finds the thumb and the separate fingers in the mesh, straightens spread fingers, curls them at three knuckles and folds the thumb across. Only the mesh changes; the skeleton stays the same. Unreal re-imports the meshes and the clips are rebuilt. Her hands are therefore always closed, which suits a fist fighter and still works for holding a sword later. The open-handed originals are kept in `<outfit folder>\openhands` and are always the input, so re-running is safe. Tune `CURL` / `THUMB` at the top of `fist_hands.py`; add `-Preview <folder>` to render colour-coded close-ups without changing anything. Frog is left out (different rig).

If you re-run `flatten_feet.ps1` on an outfit, run `fist_hands.ps1` for it again afterwards (delete its `openhands` copy first so the new flat-footed model becomes the source).

To check feet in game: `... -game -HellgirlFeetPreview` films the equipped outfit side-on and logs its stored sole heights (`FEET MESH` line in the log).

## Mini succubus (enemy)

The mini succubus is World III's mass enemy: small and flying only. She came from Meshy as an unrigged T-pose mesh. `Tools\Enemies\rig_mini_succubus.ps1` rigs her and brings her into the game in three steps:

1. **Rig (Blender, `rig_mini_succubus.py`).** Hellgirl's 24-bone skeleton is fitted onto her, so Mixamo clips would still fit her. Three wing bones are added per side (`LeftWing1`–`3`, `RightWing1`–`3`).
   - Joint heights, the arm line and the depth of each joint are measured from her mesh.
   - Her legs are modelled together. The mesh is cut along the centre line below the crotch, each leg's cut side is closed, and each side follows only its own leg.
   - The wings are cut out by position (the arm is the tube in front of the wing membrane) and weighted to their own bones.
   - The rest of the body gets Blender's automatic weights.
2. **Flying clips (Blender, `mini_succubus_flight.py`).** Keyframed directly on her rig as a few key poses blended smoothly, with the wingbeat on top:
   - **Hover:** 2 wingbeats per second, legs dangling.
   - **Fly:** leaning into the flight, legs trailing, 3 beats per second.
   - **Attack:** a claw swipe landing at 60%, where enemy attacks connect in game.
   - **Hit:** a flinch.
   - **Death:** goes limp, wings drooping.

   The poses are plain angle tables at the top of the script.
3. **Import (Unreal, `import_mini_succubus.py`).**
   - Creates `/Game/Enemies/MiniSuccubus/MiniSuccubus` with its own skeleton and physics asset.
   - Builds `M_MiniSuccubus`: her Meshy textures with normal and emission maps, two-sided for the wing membrane, plus an `EmissiveStrength` parameter.
   - Imports the clips into `Animations`, and removes clips that no longer exist there.

**In game:** she is enemy type `MiniSuccubus` (model slot in `Config/DefaultGame.ini`).
- She always flies, whatever the spawn site asks for.
- She hovers above Hellgirl and dives like the flying imps.
- Her model is at half scale (about 0.8 m) and centred on the capsule.
- No level spawns her yet.

**Checks:**
- `MiniSuccubus` (headless, at camp): three of them take off, dive and hurt Hellgirl, and die when hit.
- `-HellgirlMiniSuccubusPreview` (windowed): her clips in game, with Hellgirl for scale, saved to `Saved/Screenshots/MiniSuccubus_*.png`.
- Blender renders of clips moved onto another armature are misleading. Judge animations in game.