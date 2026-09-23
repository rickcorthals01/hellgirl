# Imp models and animations

Ground Imps use the wingless GroundImp_Rigged mesh; Flying Imps use FlyingImp_Rigged with wings. Both share a 23-bone Blender rig and the supplied painted textures. Original downloads and earlier static variants are retained.

Nine first-pass clips are connected through Config/DefaultGame.ini:

| Ground | Flying | Duration |
|---|---|---|
| Imp_Idle | Imp_Fly (hover and travel wing flap) | 1.5 / 0.5 s |
| Imp_Walk | Uses Imp_Fly | 0.8 s |
| Imp_Attack | Imp_FlyAttack | 1.25 s |
| Imp_Hit | Imp_FlyHit | 0.3 s |
| Imp_Death | Imp_FlyDeath | 0.6 s |

Attacks contact at 60% of playback, matching the existing claw damage event. Hits interrupt attacks; death takes priority and holds its final pose until despawn. Ground stride speed follows movement speed. Flying Imps flap while hovering and travelling. Movement and collisions use the existing character capsule.

Game assets: Content/Enemies/AnimatedImp, with clips in its Animations folder. Materials reference the existing Content/Enemies/GroundImp folder.

Editable source: Developer idea folder lol/Stage 01 Hands/Assets/Enemies - Demon 01 Imp/hand-painted-imp/Animations. Imp_Rig.blend holds the rest rig; individual clips have their own Blender and FBX files. Rebuild/import scripts are in Animation Testing/animate_imps.py and import_imp_animations.py.

Validation: every imported frame was compared against Blender joint positions (Unreal_checks.json beside the source animations). The editor build and unattended three-map encounter check passed. Hellgirl.Animation.ImpPlayback checks configured idle/movement/attack/hit/death selection and attack contact timing. Visual inspection used Blender stills; an interactive combat playtest is still needed to judge polish. This is a basic rig with automatic weights, simple claw strikes and bone-driven wing flaps, without finger articulation, cloth simulation or ragdoll death.
