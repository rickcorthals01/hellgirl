# Ashen Arena

**Current map:** see **STAGE ONE MAP.md**. The latest source replaces the room and waves with a larger landscape, winding road, hill, and three approach-activated enemy rifts. It also updates aerial damage and coin attraction/contact. Rebuild with Build and Open.cmd before testing.

**September 11 update:** the project is now titled Hellgirl and has a new unarmed-combat source pass. Use **FISTS PLAYTEST.md** for the current controls, implemented features, limitations, and rebuild instructions. The initial scaffold below is historical; the original sword/Space-dodge controls no longer describe the new source. The earlier baseline compiled successfully, but the new fist changes await their full Unreal rebuild.

An editable, local Unreal Engine 5 project for a third-person hack-and-slash game.

## Current status

The C++ source and project configuration are created. **This project has not yet been compiled or playtested in Unreal Engine**, because Unreal and the C++ build tools were not installed when it was generated. Treat it as a starter prototype awaiting its first build, not a finished game or a ready-to-run executable.

The implementation includes camera-relative movement, mouse look, three-hit sword combos, stamina-based dodging with temporary invulnerability, enemies that approach and telegraph attacks, health bars, five waves, healing between waves, victory/defeat messages, and restart. Characters and the arena use basic engine shapes. There are no imported character models, skeletal animations, music, or sound effects yet.

## Install the tools

1. Finish installing Unreal Engine 5 through the Epic Games Launcher. This source targets UE 5.4 or later; use the version you are installing for the initial build.
2. Install Visual Studio with **Game development with C++**, the matching MSVC compiler, and the Windows SDK. Use Epic's version table to match Visual Studio to your Unreal release: [Epic's Visual Studio setup guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine). Visual Studio is a separate application from Visual Studio Code.

## Open the project

1. Keep this entire folder together. You may move it to another folder on your PC.
2. Right-click `AshenArena.uproject` in File Explorer. On Windows 11 you may need **Show more options**. Choose **Switch Unreal Engine version**, select your installed engine, then choose **Generate Visual Studio project files** if needed. No engine version has been hardcoded into this starter.
3. Double-click `AshenArena.uproject`. If asked to build missing modules, choose **Yes**. The initial build can take several minutes.
4. If automatic compilation fails, open the generated solution in Visual Studio. Choose **Development Editor** and **Win64**, build the AshenArena project, and review the first error in the build output. Save that error so we can fix it.
5. In Unreal, press **Play**, then click inside the game viewport to capture the mouse.

The startup level is Unreal's empty Entry map. The game creates its arena when Play begins, so the environment does not appear in the editor before Play. This avoids pretending that an Unreal binary map was generated without the editor.

## Controls

| Input | Action |
| --- | --- |
| W / A / S / D | Move relative to the camera |
| Mouse | Look and aim your sword swing |
| Left click | Attack; click again after a swing to chain up to three hits |
| Space | Dodge in your movement direction, or forward when stationary |
| R | Restart the arena |
| Escape | Toggle mouse cursor capture |
| Shift + F1 | Unreal editor shortcut to release the mouse during Play |

Dodging costs 30 stamina and briefly avoids damage. The third combo hit deals extra damage. Clearing a wave restores 25 health. Beat all five waves to win.

## Work on your own game

- `Source/AshenArena/ArenaFighter.h`: starting health, sword damage, attack range, and movement speed.
- `Source/AshenArena/ArenaFighter.cpp`: movement, combat, dodge, primitive character shapes, and enemy behavior.
- `Source/AshenArena/ArenaGameMode.cpp`: generated arena, lighting, enemy counts, and waves.
- `Source/AshenArena/ArenaHUD.cpp`: on-screen information.
- `Config/DefaultInput.ini`: controls.

For visual editing, first create and save your own level under `Content/Maps`. Set it as the editor startup map and game default map in Project Settings > Maps & Modes, and retain ArenaGameMode as the default game mode. The generated arena will still appear during Play until you replace or disable `BuildArena()`.

The fighter is Blueprintable and its combat settings are exposed. You can derive a Blueprint from ArenaFighter and replace the placeholder look. To use that Blueprint as the player, select it as Default Pawn Class in a GameMode Blueprint derived from ArenaGameMode. Enemy spawning currently uses the native fighter class in `StartWave()`; update that separately when adding your enemy Blueprint.

## First playtest checklist

Once the initial build succeeds, verify movement and camera, combo hits and misses, enemy damage, dodge invulnerability and stamina regeneration, wave transitions, death, victory, and restarting. Also check whether the game's difficulty and camera feel good. Static source checks do not establish any of these gameplay results.

This is a single-player prototype. Enemy movement is direct pursuit in an open arena; navigation around complex obstacles, animation-driven weapon collision, audio, menus, saving, controller input, and packaged Windows builds are future work.
