# Dialogue

## The box

- **Layout:** the dialogue box runs along the bottom of the screen, with the speaker's name and the line. The speaker's portrait stands on top of the box, on the **left for Hellgirl** and on the **right for everyone else**. A box with no name (an unseen voice, "STOOOPPPPP!!!") is allowed.
- **Narration:** narration pages are centred text on a black screen, used for Stage 1's opening.
- **While talking:** the game pauses and the HUD hides.
- **Controls:** A, Space, Enter, E or a mouse click continues. Story conversations can't be skipped with Escape.

## Writing conversations

Conversations live in `Content/Dialogue/LevelOne.ini`, one section per story moment, with numbered pages:

```
[L1_AfterWave2]
Speaker0=Hellgirl
Mood0=Surprised
Line0=Wtf!
Speaker1=
Line1=STOOOPPPPP!!!
```

| Key | Meaning |
|---|---|
| `SpeakerN` | Who talks. Empty gives a box without a name. |
| `LineN` | What they say |
| `MoodN` | Which portrait: `Neutral` (default), `Angry`, `Surprised`, `Headache`, `Quiet`, `Smirk`, `EvilSmirk`, `Laugh`, `Hurt`, `HurtAngry`, `HurtDetermined`, `HurtNeutral`. `none` hides it. |
| `StyleN` | `Narration` for centred text on black |

## Portraits

- **Where the art comes from:** portrait PNGs live in `Desktop\Hellgirl Game\Talkbox Images`:
  - Hellgirl's are under `HellGirl\<Outfit>\`.
  - Other characters get their own folder, e.g. `Goblin Queen\`.
  - The mood comes from the file name: "angry", "surprised", "headache", "quiet", and so on.
- **Adding or changing art:** close Unreal and run:
  ```
  powershell -ExecutionPolicy Bypass -File Tools\Dialogue\import_portraits.ps1
  ```
  - **Cleanup:** it cuts away the baked-in checkerboard background along the ink outline, and saves clean copies in `Talkbox Images\Processed`. Your originals are untouched.
  - **Import:** it imports them as `/Game/Dialogue/Portraits/<Set>/T_<Set>_<Mood>`.
- **Which set is used:**
  - Hellgirl's portraits follow her equipped outfit (Rags, Succubus Armor), falling back to Rags.
  - A missing mood falls back to Neutral.
  - A character with no art (the Goblin Queen, for now) shows just their name.

## Stage 1 story (Goblins, level 1)

This follows `Developer idea folder lol\Dialog and Story\Dialog START GAME - Goblins Stage 1.txt`.
1. **Darkness:** "… … … Wake, subject." as narration, then a fade in.
2. **Waking up:** Hellgirl lies on the floor ("…", "Ugh...", "Where am I?", with the headache portrait), then gets up (the Mixamo Getting Up clip, `WakeUp`). Only then can you move.
3. **Exploring:** you walk around with no enemies. After a few steps the voice says "Prove you're worthy." and wave 1 begins.
4. **After wave 2:** "Wtf!", "Ugly creatures!", "How dare you touch me?", then STOOOPPPPP!!!, "?", then the Goblin Queen: "How dare you hurt my babies?" / "Children! Kill her!". Waves 3–5 follow.
5. **After wave 5:** "I gotta get out of here."
6. **At the portal:** choosing YES plays "Let's find a save spot and rest.." and returns to camp.

Stage 3 (the Goblin Queen fight) keeps its conversations: `Opening`, `AfterFirstWave`, `BossEntrance`, `QueenLowHealth`, `QueenReturn`, `QueenDefeat`.

## Checks

- `-HellgirlStoryCheck` on `Entry?StageMap=1?CampaignLevel=1` (`Story1` in `Tests\run-checks.ps1`) plays Stage 1 through. It verifies:
  - the conversation order and 17 pages;
  - 8 portraits;
  - Hellgirl on the floor while waking;
  - no wave before the voice.
- Add `-StoryShots` in a window to photograph every page to `Saved/Screenshots/Story`.
- On the Stage 3 URL, the same flag checks the Queen story.
