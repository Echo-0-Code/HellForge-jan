# Effects, enemies and the armory

<!-- diataxis: how-to -->

Three SDK 21 calls let a code mod act on the world around the player. `play_effect` hangs particles on an entity, on
this machine or on every machine of the lobby. `find_enemies` lists the enemies near a point. `armory_weapon` names the
weapon the armory's customize screen shows. SDK 27 adds line of sight, projectiles near a point, the side an entity
is on, and joint poses set just before a frame renders. The types are in `hellforge_world.h`, which `hellforge_mod.h`
includes.

## Contents

- [Before you call](#before-you-call)
- [An effect every player sees](#an-effect-every-player-sees)
- [Enemies near a point](#enemies-near-a-point)
- [The weapon in the armory](#the-weapon-in-the-armory)
- [Line of sight](#line-of-sight)
- [Projectiles near a point](#projectiles-near-a-point)
- [Friend or foe](#friend-or-foe)
- [Posing joints](#posing-joints)
- [Limits](#limits)

## Before you call

- The manifest needs `"sdk": 21` (27 for the SDK 27 calls), and the code checks `hf->size` before each call.
- `play_effect` and `stop_effect` run on the game thread only: in a game event, or in an `on_game_thread` callback
  ([the game thread](runtime.md#the-game-thread)). Elsewhere they are refused, and your log says so.
- `line_of_sight` and `set_projectile` run on the game thread only too.
- `find_enemies`, `find_entities`, `find_projectiles`, `faction_of`, `side_of` and `armory_weapon` may be called from
  any thread.
- `joint_pose` reads on the game thread or in an `on_render` callback.

```c
#define HAS(hf, call) ((hf)->size >= offsetof(HfApi, call) + sizeof (hf)->call)
if (!HAS(hf, armory_weapon)) { hf->log(mod, "needs HellForge with SDK 21"); return 0; }
```

## An effect every player sees

A backpack that vents steam, as a Coolant Pack does, needs three things:

1. Particles that are loaded. Load their package with `load_package`, or put them in a session package of your mod's
   data ([packages](runtime.md#packages)). Unloaded particles are refused.
2. The entity to hang them on, such as the player's entity from `HfPlayerEvent`.
3. Which node of its unit they hang from. `""` is the unit's root. A node the unit lacks is refused.

```c
static uint32_t g_steam;

static void OnSpawn(HfMod* mod, const HfPlayerEvent* e, void* user)
{
    const HfApi* hf = HF_API(mod);
    if (!e->local || !e->entity) return;            /* only the machine of the player who wears it */
    HfEffect fx = {0};
    fx.size = sizeof fx;
    fx.flags = HF_EFFECT_SHARED;                     /* every machine plays it on its own copy of the player */
    fx.particles = hf->resource_hash("<particles>"); /* the name of your particles resource */
    fx.entity = e->entity;
    fx.node = "<node>";                              /* a node of the Helldiver's unit, "" for its root */
    fx.position[1] = 0.1f;                           /* 10 cm up from the node, in the node's own axes */
    g_steam = hf->play_effect(mod, &fx);             /* 0: refused, your log says why */
}
```

Stop it when the vent closes. `HF_EFFECT_STOP` lets the particles already out fade, and `HF_EFFECT_DESTROY` removes
them at once:

```c
if (g_steam) hf->stop_effect(mod, g_steam, HF_EFFECT_STOP);
```

A shared effect is played by one machine for everyone. So only the machine that owns the event should call it with
`HF_EFFECT_SHARED`, as above. If every machine called it, each player would see the effect twice. Without the flag
the effect is this machine's only.

An effect linked to an entity goes when its unit goes, and every effect ends when the game's state changes. Its
handle then no longer stops anything. With `entity` 0 the effect plays at `position` in the world.

## Enemies near a point

`find_enemies` fills an array with the living enemies inside a radius, nearest first, and answers how many it wrote:

```c
HfFoundEnemy near[16];
float centre[3] = {x, y, z};
uint32_t n = hf->find_enemies(mod, centre, 25.0f, near, 16);
for (uint32_t i = 0; i < n; i++)
    if (!(near[i].flags & HF_FOUND_DOWNED)) hf->log(mod, "%u at %.1f m, %d health", near[i].entity, near[i].distance, near[i].health);
```

| Field | Meaning |
| --- | --- |
| `entity`, `unit` | the enemy's entity id and its unit's handle |
| `resource` | its type, as `HF_ENTITY_...` or an entity a mod added |
| `position`, `distance` | its unit's root in the world, and how far it is from `centre` |
| `health`, `max_health` | its health pool now, and its full pool (-1 when that does not read) |
| `faction` | its faction mask. Bit 0 is the Helldivers' side |
| `flags` | `HF_FOUND_DOWNED`, and `HF_FOUND_AUTHORITY` when this machine simulates it |

An enemy is any entity whose faction is not the Helldivers' side. An entity with no faction counts when its type is
an enemy type. Only a mission has enemies; elsewhere the call answers 0. It reads every health record, so call it a
few times a second at most, not every frame.

## The weapon in the armory

`armory_weapon` gives the loadout id (`HF_ITEM_...`) of the weapon the armory works on. It reads two screens:

| `menu` | Screen | The id |
| --- | --- | --- |
| `HF_MENU_ARMORY` | the Armory's grid, with CUSTOMIZE and EQUIP | the item selected in it, the one CUSTOMIZE would open |
| `HF_MENU_CUSTOMIZE` | the customize screen of one weapon | the weapon it shows |

On any other menu, or with nothing selected, it answers 0. On the Armory's armour tabs the id is the armour piece's.
Its `menu` argument, which may be `NULL`, gets the menu state on top:

```c
uint32_t menu = 0, weapon = hf->armory_weapon(mod, &menu);
if (menu == HF_MENU_ARMORY && weapon == HF_ITEM_AR23_LIBERATOR) hf->set_status(mod, "Liberator selected");
```

Call it again whenever you need it: the screens change weapon without telling your mod. The Armory's answer follows a
click or the gamepad's selection, not the mouse hovering over an item.

## Line of sight

`line_of_sight` casts the engine's physics ray from one point to another, as a turret checks it can see its target.
It needs `"sdk": 27`, and runs on the game thread in a mission:

```c
HfSight how = {0};
how.size = sizeof how;
how.ignore[0] = turretUnit;   /* the turret's own body */
how.ignore[1] = wearerUnit;   /* the Helldiver who carries it */
how.skip_to = 1.0f;           /* the last metre is the target itself */
HfSightHit hit;
uint32_t seen = hf->line_of_sight(mod, muzzle, target, &how, &hit);
if (seen == HF_SIGHT_BLOCKED) hf->log(mod, "blocked %.1f m out by unit %u", hit.distance, hit.unit);
```

| Answer | Meaning |
| --- | --- |
| `HF_SIGHT_CLEAR` | nothing that counts lies on the line |
| `HF_SIGHT_BLOCKED` | `hit` gets the first hit that counts: its distance, the unit (0 for terrain) and where |
| `HF_SIGHT_UNKNOWN` | the ray was not cast now; your log says why |

`HF_SIGHT_UNKNOWN` comes most often while the AI's own rays are being cast, which share the scene. Ask again next
update, or decide what an unknown answer means for your mod: a point-defence turret may fire anyway.

## Projectiles near a point

`find_projectiles` lists the shots in flight within a radius, nearest first, of one type or of any (`type` 0).
`set_projectile` moves one, by the `slot` it was found in:

```c
HfFoundProjectile shots[32];
uint32_t n = hf->find_projectiles(mod, turret, 60.0f, 0, shots, 32);
for (uint32_t i = 0; i < n; i++)
    if (shots[i].type == rocketType)
    {
        float slow[3] = {shots[i].velocity[0] * 0.02f, shots[i].velocity[1] * 0.02f, shots[i].velocity[2] * 0.02f};
        hf->set_projectile(mod, shots[i].slot, shots[i].type, NULL, slow); /* a burned-out rocket falls */
    }
```

| Field | Meaning |
| --- | --- |
| `slot`, `type` | its place in the projectile pool, and its `ProjectileType` |
| `position`, `velocity`, `distance` | where it is, how fast it moves (m/s), and how far it is from `centre` |
| `source` | the entity that fired it, 0 when none reads |
| `peer` | the peer of the player its hits are credited to, 0 for none |

Shots that already hit are left out. `set_projectile` refuses a slot that no longer holds a flying shot of that type,
so a slot used again by another shot is safe. It moves this machine's copy only: run it on every machine, or on the
host when the shot's effect is the host's.

## Friend or foe

`faction_of` gives an entity's faction mask, and `side_of` says which side it is on, seen from a mask:

```c
uint32_t side = hf->side_of(mod, entity, HF_FACTION_HELLDIVERS);
if (side == HF_SIDE_HOSTILE) aim = 1;
```

The rule is the game AI's own. No mask is `HF_SIDE_NEUTRAL`, a mask sharing a bit is `HF_SIDE_FRIENDLY`, any other
mask is `HF_SIDE_HOSTILE`. Seen from the Helldivers, an enemy type with no mask is hostile, as `find_enemies` counts
it. A drone of yours sees with its own mask: `hf->side_of(mod, entity, hf->faction_of(mod, drone))`.

`find_entities` lists the living entities of any sides, as `find_enemies` lists enemies. A medic drone finds hurt
teammates like this:

```c
HfFind q = {0};
q.size = sizeof q;
q.sides = HF_FIND_FRIENDLY;
q.flags = HF_FIND_HURT | HF_FIND_NOT_DOWNED;
memcpy(q.centre, drone, sizeof q.centre);
q.radius = 20.0f;
HfFoundEnemy hurt[8];
uint32_t n = hf->find_entities(mod, &q, hurt, 8);
```

## Posing joints

A turret head that turns, or a suit's limb that follows its wearer's bone, is a joint your mod sets each frame. Set it
after the game has animated the units and before the frame renders, or it trails a frame behind. `on_render` calls
you back at that moment:

```c
static void OnRender(HfMod* mod, void* user)
{
    const HfApi* hf = HF_API(mod);
    float hand[16];
    if (!hf->joint_pose(mod, wearerUnit, hf_node_id(mod, "<bone>"), HF_POSE_WORLD, hand)) return;
    HfJointPose head = {{1, 0, 0, 0, 1, 0, 0, 0, 1}, {0, 0, 0.4f}, {1, 1, 1}, 0};
    /* ... turn head.axes towards the target, using hand ... */
    hf->pose_joint(mod, packUnit, hf_node_id(mod, "<joint>"), &head);
}
hf->on_render(mod, OnRender, NULL); /* once, from hellforge_mod_init */
```

- `pose_joint` takes the joint's pose in its parent's frame: three axis rows, the offset from the parent, the scale.
- In an `on_render` callback it applies at once. Called elsewhere, such as in `on_game_thread`, it applies at the next
  frame's render.
- HellForge updates each unit you posed once, after every mod's callback.
- Animated joints are set again by the game each frame, so pose them every frame.
- `joint_pose` reads a joint's world pose (a 4x4 row-major matrix, position at `[12..14]`) or its local pose.
- Node ids are `hf_node_id(mod, "<name>")`; 0 is the unit's root.

## Limits

- 256 effects at once across all mods and players. A new one past that takes the oldest one's place in the table, and
  the oldest can then no longer be stopped.
- 64 shared effect messages a second from one machine. Past that, the effect plays on this machine only, and your log
  says so.
- A shared effect needs an entity the other machines know: a networked one, such as a player. An entity with no
  network id is refused.
- The other machines need the same particles loaded. Mod sync makes every player run the same mods.
- 32 `on_render` callbacks across all mods, and 1024 poses queued outside them a frame.
- Only the reviewed game build (25480438). On another, these calls are refused and `modloader.log` says why.
