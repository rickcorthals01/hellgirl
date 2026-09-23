# Enemy colours

Ground Imps: orange-red body, yellow eyes, brown cloth, ivory teeth.
Flying Imps: blue-violet body, purple wings, yellow eyes, brown cloth, ivory teeth.
Gulps: red skin, cream belly, pink tongue, ivory teeth/spikes, dark claws and mouth.

Materials are saved in Content/Enemies/EnemyColours and assigned to the three active skeletal meshes. Imp materials retain the source texture shading under the palette. Gulp colours use explicit material values so they do not depend on imported vertex colours.

Fixed the import helpers to write edited material structs back into their arrays. All 22 material assignments were verified in a fresh Unreal process. Run color_enemies.py after reimporting models to restore these custom palettes. The Blender previews predate this Unreal colour pass.
