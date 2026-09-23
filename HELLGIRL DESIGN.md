# Hellgirl — working design brief

Based on the two text descriptions and all 13 JPG references in `C:\Users\rickc\Desktop\Hellgirl Game`, reviewed September 10, 2026. The user confirmed eight combat stages plus an intro and outro. Original reference files remain in their Desktop folder.

## Game concept

Combat requirements are expanded in `HELLGIRL COMBAT.md`, based on the subsequently added `Combat Design info.txt` and `Stage 01 Hands/Hands Weapon info.txt`. Stage reference folders have since been renamed to `Stage 01 Hands` and `Stage 02 Sword`, and `Stage 03 Upgrade Sword` has been added.

Hellgirl is absorbed into a hellworld. Explore and fight within large, square, confined stages. Meeting a stage's enemy-defeat or traversal objective causes its boss to spawn. Defeating the boss completes the stage and enables progression. Completed stages remain unlocked for replay, high scores, coins, or XP. Weapon upgrades persist across stages.

## Progression

| Experience | Planned progression |
| --- | --- |
| Intro | Apartment point-and-click scene; TV starts Stage 1 |
| Stage 1 | Hand-to-hand combat |
| Stage 2 | Find and use a sword |
| Stage 3 | Unlock special sword abilities |
| Stage 4 | Find a gun; freely switch between sword and gun |
| Stage 5 | Unlock special gun abilities |
| Stage 6 | Find and use the CAR; specific mechanics not yet described |
| Stage 7 | Unlock the DEMON FORM ultimate ability |
| Stage 8 | Defeat the final boss |
| Outro | Ending experience; details not yet described |

## Apartment intro — supplied requirements

At night, Hellgirl sits on the couch in a small, dirty apartment wearing her sweatpants outfit. The bathroom is the only other room and is partially out of view. Empty chip bags, fast-food cups, and unpaid bills contribute to the clutter. The large box TV provides the only illumination, in blue.

Use a fixed, first-person seated view. Her legs rest on the long coffee table. The player cannot walk around. The table holds magazines and an ashtray with a smoking cigarette. The TV sits beyond it.

| Click target | Response |
| --- | --- |
| Ashtray | Smoke puffs and one of the supplied remarks |
| Magazine | Top magazine pages flip and one of the supplied remarks |
| TV | Display `START THE GAME?` with YES and NO choices |
| YES | Transition to Stage 1 |
| NO | Stay in the apartment and allow further interaction |

Ashtray lines, preserved from the source:

- I should probably quit.
- Smoking kills.
- Come to mama.

Magazine lines, preserved from the source:

- I wish I looked like that.
- Did you know famous celebrity is breaking up with other famous celebrity? What is the world coming to.
- I should probably burn these.

## Visual reference inventory

These are reference images, not rigged models or ready-to-use Unreal assets. The observations below describe visual cues, not additional gameplay requirements.

| Folder | References reviewed | Observed direction |
| --- | --- | --- |
| Intro Stage | Sweatpants.jpg | Loose gray sweatpants, dark fitted top, casual shoes, necklaces |
| Stage 01 / Player | Hellgirl concept.jpg; Hellgirl concept face.jpg | Dark tousled hair, tattoos, white top; character appearance references |
| Stage 01 / Enemies | Imp Enemy.jpg | Small red winged demon, horns, pointed tail, yellow eyes |
| Stage 01 / Enemies | Slurp Enemy.jpg | Squat red monster, large mouth, long tongue, back spikes |
| Stage 01 / Enemies | Succubus Enemy.jpg | Purple humanoid demon, pale hair, horns, wings, pointed tail |
| Stage 02 / Player | Hellgirl Battle Outfit.jpg; Hellgirl Battle Outfit 02.jpg | Dark streetwear, loose trousers, belts and layered tops |
| Armour and skins | Elfmail 01.jpg; Hellgirl Metal Skin.jpg | Chainmail/fantasy and black metal-fashion variants |
| Weapons | Hellgirl Sword 01.jpg | Straight blade, curved crossguard, dark grip |
| Hellguy | Hellguy.jpg; Hellguy Sword.jpg | Dark-haired, tattooed male character; sword reference; role unspecified |

The collection combines photographic fashion references and stylized illustrations. Final rendering style has not been selected. A gothic alternative-fashion direction is an interpretation of the references.

## Proposed first playable milestone

Build a short connected sequence: apartment interaction → TV confirmation → Stage 1 arena → unarmed fight → boss → stage-complete screen. Initially use clearly identified placeholder geometry and one enemy type to validate the loop. Add a saved Stage 1 completion flag and replay entry before expanding to later stages.

This is a proposed implementation order. Stage 1's environment, boss design, precise spawn objective, and enemy behaviors remain unspecified. Do not invent final versions of those details from the reference images alone.

## How this changes the current starter

Latest map pass: see **STAGE ONE MAP.md**. The source now implements a landscape-shaped graybox based on Map idea Stage 01.png and fixed proximity-activated spawn sites. This supersedes the old five-wave scaffold described below; the boss and stage progression remain future work.

The existing AshenArena source is an uncompiled generic combat scaffold. It currently starts with a sword and uses five waves. Those mechanics do not yet implement the Hellgirl progression.

The next implementation should introduce a separate apartment interaction mode, unarmed Stage 1 combat, a stage objective and boss flow, and persistent progress. The sword should become a Stage 2 unlock. The current five-wave victory condition must be replaced with stage-specific completion. The generic project title can be changed to Hellgirl when the game-specific implementation begins.

No Hellgirl gameplay or reference images have been integrated into the executable by creating this brief. Compilation and playtesting remain pending installation of Unreal and its build tools.

## Details to resolve as their stages are built

- Stage 1 environment, objective, boss, and enemy attack patterns.
- Whether CAR means a drivable vehicle and how it works within confined stages.
- Hellguy's story and gameplay role.
- Sword/gun special abilities and demon-form duration, cost, and effects.
- Scoring, coins, XP, upgrade economy, and replay rules.
- Transition into the hellworld, final boss, and outro.
- Final character design and rendering style.
