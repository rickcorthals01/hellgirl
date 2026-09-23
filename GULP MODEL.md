# Gulp model — first pass

Reference: Developer idea folder lol/Stage 01 Hands/Assets/Enemies - Demon 02 Slurp/Slurp Enemy.jpg.

Editable Blender model, FBX and front/back/three-quarter previews are in that folder's Model subfolder. Gulp.blend contains the model, the 19-bone rig and a preview studio. The original reference is unchanged. The mouth is recessed geometry with separate teeth, lower jaw and long tongue. Jaw, tongue and limbs have initial skin weights for later animation.

The regular Gulps slot in Config/DefaultGame.ini uses /Game/Enemies/Gulp/Gulp.Gulp at 0.9 scale, facing the existing movement direction and offset to capsule feet. GulpBoss remains unchanged. Existing movement, damage and collision behavior is retained. No Gulp animation clips are connected yet.

Seven materials use exported vertex colors for red skin, darker lips/mouth, pink tongue, cream belly, ivory teeth/horns and dark claws. This is an editable first model pass, with separate intersecting limb forms and basic generated skin weights. Further sculpting, animation deformation checks and mesh reduction/LODs are still appropriate before final production use. Current source geometry: 96,300 vertices / 191,624 triangles.

Import report: Unreal_import_checks.json next to Gulp.blend. Runtime test: Unreal Project/GulpModelRuntime.log. The build/import helpers live in Animation Testing/build_gulp.py and import_gulp.py. Previews are Blender renders, not in-game screenshots.
