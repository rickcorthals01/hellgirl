# Kicks and subtle secondary motion

The light combo now has right punch, left punch, right kick, then left kick. Queue the next left-click near the end of the current attack's recovery. The right kick lasts 0.43 seconds; the left kick lasts 0.48 seconds. The kick contact poses are within 4 ms of the existing damage timings. These are first-pass forward kicks with a chamber and recovery, not spinning or aerial kicks.

Hair and subtle body motion are enabled. Damped springs respond to movement acceleration, animated torso/head movement, and landings. Body motion uses the existing breast bones and is limited to 4.5 mm; hair motion affects the upper bun/tips through two added shape controls and is limited to 6.5 mm at full influence. The scalp stays anchored. Large movement discontinuities and long frame hitches reset the springs. These small effects do not simulate cloth or body-to-clothing collision.

To adjust the effect, use Unreal's console:

- `hellgirl.BodyMotion 0` disables body motion; `0.5` halves it; `1` restores the default.
- `hellgirl.HairMotion 0` disables hair motion; `0.5` halves it; `1` restores the default.

The original character mesh is retained. The player now uses `Content/HellgirlTest/Hellgirl_Secondary.uasset`, on the same skeleton, with the same nine material assignments and facial expressions. The extra hair controls were added in an editable copy, `Hellgirl_SecondaryMotion.blend`, under the purchased model folder.

Kick Blender files, FBX exports, preview images and pose-comparison measurements are under `Developer idea folder lol/Stage 01 Hands/Assets/Player Character - Hellgirl/Animations/Kicks`. The imported kick clips are in `Content/HellgirlTest/Kicks`.

Validation: the game build passed; all 44 right-kick and 49 left-kick samples matched the Blender joint positions within 0.005 mm. Unreal's actual-character test confirmed body-bone displacement and hair-curve response within their limits. The spring tests passed at 30, 60 and 144 fps, including settling and hitch reset. Interactive combat feel and rendered secondary-motion appearance still need playtesting; deep hip creasing and the finger/thumb poses remain polish work.
