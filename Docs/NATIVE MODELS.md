# Native model update — September 16, 2026

Hellgirl uses the supplied Default Rags and Default Succubus Armor meshes with their original geometry, skin weights and skeletons. The old Hellgirl rig, secondary-motion layer and retargeted combat animations are disconnected. Her supplied walk/run animations and a temporary relaxed neutral pose are active. Combat mechanics remain playable, but combat/jump/dodge poses are placeholders awaiting new animations authored on these rigs.

Pause with Esc/Start, then choose RAGS or SUCCUBUS ARMOR. The choice applies immediately and is saved in local game settings for future sessions and map travel. It does not change health, energy or damage.

Ground Imps use the new Imp model and its adapted animation set. FlyingImps remain unchanged. Rat, Goblin male/female and Frog files have not been installed as enemies in this change.

Source models in Meshy Models are untouched. Active Hellgirl preparation: work/prepare_fresh_hellgirl.py; import: work/import_fresh_hellgirl.py. Animation Testing/FreshHellgirl contains the native-rig working files. Earlier NewModels and retargeted Hellgirl NativeModels outputs are rejected experiments and must not be imported.

Validation: editor build succeeded; rendered pause-menu check passed both native outfit swaps, saved selection, unchanged combat state and resume. Native-rig model previews were inspected.

Additional outfits: Ghost, Frog, Goblin Queen, Imp Mother and Rat. All seven are available through the pause-menu outfit selector and use their own supplied rigs, walk/run clips, and color materials. New combat animation authoring remains pending. Extra-skin preparation/import scripts: work/prepare_extra_skins.py and work/import_extra_skins.py.

Seven-outfit validation: editor build succeeded; runtime pause check passed all seven model/animation loads, nonempty material assignments, saved selections, unchanged health/energy and resume.
