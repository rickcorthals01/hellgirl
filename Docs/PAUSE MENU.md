# Pause menu

Press P, Escape, or controller Start to pause. In an embedded Unreal Play session, use P or Start because the editor may reserve Escape for stopping Play.

The game freezes beneath a dim translucent overlay and a transparent rose-and-vine frame around the entire screen using the supplied gothframesf.jpg reference. The paper background is removed at runtime; only pale gold ornament remains.

- PLAY: click, press Enter, or controller A to resume.
- OPTIONS: coming-soon placeholder.
- MENU: coming-soon placeholder.
- Escape, P, Start, or controller B also resumes while the overlay is focused.

The menu opens with PLAY focused. Gameplay input is blocked while paused. Held charge, block, sprint, slow-walk, and queued attack input are cleared so they do not stay held after resuming. Mouse capture returns to gameplay on resume.

The original reference is preserved. The game uses Content/UI/Pause/GothicFrame.jpg, included in packaging as a runtime UI resource. Menu code lives in HellgirlPlayerController.cpp.

Validation: editor target compiled; a rendered runtime check verified the frame loads, simulation time remains frozen while paused, the cursor is shown, and gameplay time resumes afterward. Preview: Saved/PausePreview.png.

