# Gothic dialogue panel

Transparent dark tint with pale gothic ornament matching the pause menu. Speaker name and wrapped/scrollable dialogue on the left; a square portrait area on the right. The panel scales down on smaller windows. No portrait artwork has been assigned yet; the square remains empty when none is supplied.

Interact with the forest merchant to see the panel. Continue opens his shop; Escape or controller B dismisses the conversation. Mouse click, Enter, Space, E or controller A continues. Gameplay pauses and combat input is cleared while dialogue is open.

Reusable Blueprint-callable controller methods: ShowDialogue(Speaker, Line, optional Texture2D Portrait), CloseDialogue. The current merchant greeting is placeholder text. Level 1 now uses multi-page conversations from Content/Dialogue/LevelOne.ini, transcribed from the user's Dialog When.txt. A / Space advances one page. Opening (4), after wave one (2), boss entrance (1), Queen at 30% (1), Queen final comeback at 50% (1), and defeat (15): 24 pages total. Story conversations stay paused until finished; Escape does not skip them. The Queen surrenders instead of ragdolling, then runs away after the final page. Replay Level 1 to replay the story. Portrait artwork remains unassigned.
