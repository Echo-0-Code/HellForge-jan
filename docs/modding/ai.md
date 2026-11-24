# Orders for AI units

<!-- diataxis: how-to -->

SDK 31 lets a code mod give AI units orders. A unit keeps the game's own brain: it aims, fights and walks with its
own animations. HellForge only tells its path agent where to go, through the game's own movement call. Orders come
from the comms wheel's Follow me and Hold position, or from the mod. The types are in `hellforge_ai.h`, which
`hellforge_mod.h` includes.

## Contents

- [Before you call](#before-you-call)
- [Put a unit on the players' side](#put-a-unit-on-the-players-side)
- [Obey the comms wheel](#obey-the-comms-wheel)
- [Give orders from code](#give-orders-from-code)
- [Hear the wheel's orders](#hear-the-wheels-orders)
- [When a point cannot be reached](#when-a-point-cannot-be-reached)
- [Co-op](#co-op)
- [Which units can be ordered](#which-units-can-be-ordered)

## Before you call

- Check `hf->size >= offsetof(HfApi, ai_state) + sizeof(void*)`; an older HellForge has none of these calls.
- Set `"sdk": 31` in `hellforge-mod.json`.
- Call `ai_obey` from `hellforge_mod_init`; call `ai_order` on the game thread.

## Put a unit on the players' side

A unit's side is its `FactionComponentData` record (`HfFaction`). Give your entity copy a record of its own that
says 1 (Helldivers). The game reads it when the unit spawns, so the unit is friendly for its whole life.

```c
static const HfSet friendly[] = {{HF_FIELD_AT(Faction, factions, 0), HF_U32(1)}, {0}};
static const HfEntityDef walker = {.size = sizeof(HfEntityDef), .name = "my_name/my_mod/walker",
    .base = HF_ENTITY_SCOUT_STRIDER, .set = friendly};
uint64_t entity = api->register_entity(mod, &walker);
```

- The factions are bits: 1 Helldivers, 2 Terminids, 4 Illuminate, 8 Automatons, 16 wildlife.
- Do not change a living unit's side in the game's faction table. The game writes it back within seconds, and an
  enemy squad's member that changes side is removed.
- Enemies shoot a friendly unit, and its brain picks enemies as targets.
- A copy of an enemy needs the enemy's package loaded, or the game crashes when it spawns. Enemies have no loadout
  package, so call `load_package` when a mission starts (`hf_on_mission_start`); it was resident 1 s later in tests.
  Asking for an enemy package on the ship froze the war table after the mission pick. The Scout Strider and its pilot
  are in `0x094394C2A2C22F36`.
- A Scout Strider copy aims but never fires with its native aim (`TargetingComponentData` +8 damping 1, +12 spring
  10). Damping 10 and spring 25 on the copy's own record let it fire. Its gun is a mount-slot child
  (`2a346bf4552997e2`) with no faction record of its own, so it follows the walker. Its hits carry the gun's network
  id as dealer: match a unit's hits by its children too.

## Obey the comms wheel

`ai_obey` makes every unit of an entity obey Follow me and Hold position. No other code is needed.

An obeying unit's melee abilities (damage kind 6, such as the Scout Strider's kick) deal nothing to its own side. The
brain swings them at whatever stands in reach, so without this a unit that follows at a few metres kicks the Helldiver
it follows. Its gunfire and explosions still hit friends in the way, as a sentry's do.

```c
HfAiObey obey = {sizeof obey};
obey.entity = entity;
obey.follow_distance = 8.0f;
api->ai_obey(mod, &obey);
```

| Field | Default | What it does |
|---|---|---|
| `range` | 60 m | A unit obeys a speaker this close |
| `follow_distance` | 6 m | Follow me keeps it this far from the speaker |
| `hold_radius` | 3 m | It may stray this far from its hold point |
| `rider` | none | A seated pilot's entity: the nearest one within 8 m gets the same moves |
| `start_order` | none | `HF_ORDER_FOLLOW`: a new unit follows the nearest Helldiver |

Units with a S.E.A.F. brain (615 to 631) answer the wheel themselves, so `ai_obey` leaves them alone.

## Give orders from code

`ai_order` gives one unit an order. `ai_state` says how it goes.

| Order | What the unit does |
|---|---|
| `HF_ORDER_FOLLOW` | Keeps `follow_distance` from `target`; fights while it is close |
| `HF_ORDER_HOLD` | Stays at `point` (NULL: where it stands) and walks back when pushed off |
| `HF_ORDER_GOTO` | Walks to `point`, then holds there |
| `HF_ORDER_NONE` | Hands the unit back to its own brain |

```c
float there[3] = {120.0f, 40.0f, 6.0f};
if (api->ai_order(mod, unit, HF_ORDER_GOTO, 0, there) == 0)
{
    HfAiState s = {sizeof s};
    api->ai_state(mod, unit, &s); /* s.status: HF_AI_MOVING, then HF_AI_THERE */
}
```

## Hear the wheel's orders

`hf_on_order` calls back for each Follow me and Hold position, on every machine. `HfOrderEvent` names the speaker's
Helldiver, its peer and position, and whether it was this machine's player.

```c
static void Ordered(HfMod* mod, const HfOrderEvent* e, void* user)
{
    if (e->order == HF_ORDER_FOLLOW) HF_API(mod)->ai_order(mod, g_unit, HF_ORDER_FOLLOW, e->speaker, NULL);
}
hf_on_order(mod, Ordered, NULL, 0);
```

## When a point cannot be reached

HellForge hands the path agent a point again only when the point has moved, so a long path gets time to finish. A
unit that makes no headway for 4 s goes through these steps:

1. It walks to a point halfway along the straight line.
2. It tries points 6 m to either side of that line.
3. It stands still with `HF_AI_UNREACHABLE` and tries again after 10 s, or sooner when the point moves 5 m.

The unit's log lines name each detour and give its path agent's goal.

## Co-op

- Each machine orders only the units it simulates; for a mission's units that is the host.
- An order from any player reaches every machine. The game sends its own comms message in some missions only, so
  HellForge sends the wheel's orders on the mod channel too. Each order is raised once.
- `ai_order` on a machine that does not simulate the unit is refused: send the request to the host with `send`.

## Which units can be ordered

A unit needs a brain and a navmesh path agent. These come with the unit it copies.

| Base | Path agent |
|---|---|
| S.E.A.F. troopers, Helldivers | Yes |
| Scout Striders, Automaton infantry and walkers | Yes |
| Sentries, emplacements, props | No: `ai_order` is refused |

A model on a copied unit must be rigged to that unit's skeleton, because the brain sends animation events its body
plan expects.
