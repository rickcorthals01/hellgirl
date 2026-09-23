# Enemy moveset playtest

September 15, 2026. The latest balance pass reduces Hellgirl's area damage and makes active boss attacks uninterruptible. The standalone combat-rule test, editor build, combat balance, enemy moveset, energy, physics, counter and three-map progression checks passed. Earlier enemy moveset checks are recorded below. Combat balance and visual readability need hands-on feedback.

## What changed

Ground Imps circle Hellgirl, make short forward pounces and close in for claws. FlyingImps circle above the floor, commit to a dive and remain low during recovery before returning to their hover. Attacks lock their direction when the tell begins, so moving aside can make them miss. Movement checks the floor ahead and sweeps against obstacles.

The Imp Commander has a broad forward cleave, a forward rush and a circular ground slam. At 50% health or less (225 of his starting 450 HP), he permanently enters phase 2 and can call ground Imps. A call has a 1.8-second windup and 1.2 seconds of recovery. The base summon cooldown is 18 seconds from the start of a call; attack availability can delay the next call. At most three summoned Imps can be alive, and subsequent calls fill the available spaces.

The Commander, GulpBoss and SuccubusBoss take damage during their attacks but cannot be staggered, knocked down, launched or interrupted until the action ends. This protection includes the Commander's summon and its recovery. A timed dodge remains an ordinary dodge against an attacking boss; it cannot trigger an interrupting counter. Idle bosses can still stagger, and lethal damage or a reset can stop an action. A summon has orange floor markers and a HUD warning. Summoned Imps use the normal Imp model, combat and coin rewards. Any surviving reinforcements must be defeated after the Commander dies before the portal opens.

Nearby castle enemies share a limit of two simultaneous attacks, with at least 0.45 seconds between attack starts. Regular enemies retain the 0.3-second counter window; boss flashes still warn of an incoming hit. Imps and FlyingImps use their existing animation assets; the Commander retains his placeholder with different poses and tells. Gulps and Succubus types retain their previous moves.

## Initial tuning

| Move | Windup to contact | Damage before blocking |
| --- | ---: | ---: |
| Imp claw | 0.72 s | 11 |
| Imp pounce | 0.93 s | 13.2 |
| FlyingImp dive | 1.32 s | 14.3 |
| Commander cleave | 1.20 s | 22 |
| Commander rush | 1.56 s | 26 |
| Commander ground slam | 1.68 s | 20 |

Commander slam radius is 500 cm. Ground Imps select pounces only within 550 cm so their maximum travel and hit range can reach a stationary target. All damaging attacks have recovery time and respect walls. Commander phase threshold, summon limit and interval are editable in the fighter's **Enemy > Commander** properties; individual attack tuning is in `Source/AshenArena/EnemyMoveset.cpp`.

Hellgirl's charged strike deals 12–28 base damage, aerial slam 16 and crash kick 14. Damage is full within the inner quarter of the radius and falls to 55% at the edge. All collision damage caused by one such attack shares a limit of 6 additional damage per victim, including secondary chains and wall impacts. Launch force, radius, basic attacks and energy costs remain unchanged. Even a full charge with maximum riposte plus its entire collision allowance deals at most 62 damage to one target, below a fresh Imp's 67 HP.

## Try in game

Use **LMB / X** for normal attacks, **RMB / Y** for heavy attacks, **Shift / B** to dodge, **Space / A** to jump, and **Q / LB** to block. **Esc / P / Start** pauses; **R** restarts. Accept the fire-orb fight with **E / D-pad Up** after clearing the castle encounters.

1. Let each Imp move hit once, then sidestep the same tell. Check that pounces and dives travel toward the original position, stop at walls and leave time to retaliate.
2. Dodge during a regular Imp's weapon flash. The counter should interrupt and launch it. Also interrupt a moving Imp with an ordinary punch and a strong launch; its previous attack must not resume or deal delayed damage.
3. Fight the Commander through all three attacks. Test the slam from behind him, just outside its ring and across a wall. Strike or launch another enemy into him during a tell: his health should fall, but the move must continue. A late dodge should evade without countering him. Check the same attack protection on the other two bosses and confirm that idle bosses can still stagger.
4. Lower the Commander below half health. Hit him during a call and confirm that it finishes unless he dies. Check the warning, three-Imp limit, replacement summons and pause/resume during the tell.
5. Defeat the Commander while some summoned Imps remain. The objective should name the remaining Imps and keep the portal closed until the last one dies. Verify normal coins and no further calls after his death.
6. Use one charge or aerial slam against a fresh group near a wall. Enemies should scatter while retaining health, including when their launched bodies collide repeatedly. Weakened enemies can still die, and another attack has its own damage allowance.

## Verification and known visual issue

Before the latest balance pass, the editor build and `-HellgirlEnemyMovesetCheck` passed phase transition, spawning, cap/refill, the then-current interruption rules, encounter tracking, rewards, movement, counters and attack coordination. The complete three-map journey and energy carryover/reset passed again, as did physics, energy and counter regressions. A rendered preview showed the Commander's second phase, three summoned Imps and the boss HUD. Expanded contact checks also passed all five new damaging moves, single-hit timing, rear/range/wall coverage for the slam and the blocked rush. The new boss-protection and shared collision-budget runtime checks passed, along with the updated enemy moveset, energy, physics, counter and three-map progression checks; the standalone `FistCombatRulesTest` and editor build also passed.

The preview also exposed an existing `ImpEnemyColours` material error involving the desaturation input; affected Imps can use a grey fallback material. Art assets have been left unchanged while they are developed separately. This visual issue does not invalidate the confirmed gameplay checks.
