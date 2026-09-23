# Enemy identities and model slots

Each spawned enemy now has a stable EnemyType, an EnemyType.* actor tag, an editor actor label, and a name above its placeholder. Identity stays the same if a flying enemy is knocked down.

| Map | Regular enemies | Boss |
| --- | --- | --- |
| 01 | Imps, FlyingImps | ImpCommander |
| 02 | Gulps | GulpBoss |
| 03 | Succubus | SuccubusBoss |

ImpCommander is the existing commander encounter. SuccubusBoss is the queen encounter. Map 02 now uses the user's Gulps/GulpBoss names in the encounter labels and objective.

## Assign models later

Open Unreal's Project Settings, then Game > Hellgirl Enemy Models. There is a separate model slot for every type above. Select an imported skeletal mesh in its Mesh field. Leave it empty to retain the placeholder.

Each slot also has an optional compatible Animation Blueprint, model offset/rotation/scale, and an Attack Flash Socket name for the weapon or striking hand. Set the socket when integrating a model so the counter flash follows its animated hand or weapon. Without an Animation Blueprint the model uses its reference pose; animation integration still needs to be authored for that model.

Settings are saved in Config/DefaultGame.ini. Restart Play after assigning or changing a slot. All future spawns of that type use the assigned model automatically; no individual enemy replacement is necessary. Model transforms are relative to the existing character capsule, and boss actor scale is also applied.

Spawn-site identity is separate from movement and combat: changing a model does not change enemy counts, flight behavior, boss stats, or progression. The existing placeholders remain until assets are assigned.

Implementation: EnemyTypes.h defines the identities and settings; each spawn site selects a ground/flying identity; ArenaFighter applies the corresponding appearance. The map journey check now verifies identities on every spawned group and boss.
