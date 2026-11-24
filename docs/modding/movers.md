# Movers

<!-- diataxis: how-to -->

SDK 35 lets HellForge move a unit itself: a sentry that walks, a cart that drives, any object your mod spawns. You
describe how it moves once with `add_mover`. A driver then gives it a throttle and a steer: your code with
`mover_input`, or the custom AI with `ai_drive`. The types are in `hellforge_move.h`, which `hellforge_mod.h` includes.

## Contents

- [Before you call](#before-you-call)
- [Describe how it moves](#describe-how-it-moves)
- [Drive it from code](#drive-it-from-code)
- [Let the custom AI drive it](#let-the-custom-ai-drive-it)
- [Read how it moves](#read-how-it-moves)
- [What HellForge does each update](#what-hellforge-does-each-update)

## Before you call

- Check `hf->size >= offsetof(HfApi, ai_drive) + sizeof(void*)` and set `"sdk": 35` in `hellforge-mod.json`.
- Call `add_mover` and `ai_drive` from `hellforge_mod_init`.
- Movers run on the host. The unit's moves reach the other players as its root's moves do.
- The game's own physics no longer move the unit. Leave out any movement code of your own for it.

## Describe how it moves

```c
HfMoverDef walker = {sizeof walker};
walker.entity = MY_SENTRY;
walker.mode = HF_MOVE_WALK;
walker.speed = 3.5f;
walker.turn = HF_TURN_SPOT;
walker.step_up = 0.6f;
api->add_mover(mod, &walker);
```

| Field | Default | What it does |
|---|---|---|
| `mode` | `HF_MOVE_WALK` | `HF_MOVE_WALK` steps to either side of a block. `HF_MOVE_DRIVE` goes only where it faces |
| `speed` | 4 m/s | Top speed forward |
| `reverse_speed` | half of `speed` | Top speed backward |
| `acceleration` | 8 m/s² | How fast it speeds up and slows down |
| `turn` | `HF_TURN_SPOT` | `HF_TURN_SPOT` turns standing still. `HF_TURN_VEHICLE` turns only while it moves |
| `turn_rate` | 120 °/s | Its fastest turn |
| `turn_radius` | 4 m | `HF_TURN_VEHICLE`: its tightest circle |
| `step_up` | 0.5 m | The highest step it climbs |
| `step_down` | 1 m | The deepest drop it steps down |
| `reach` | 0.6 m | How far its body reaches ahead of its root: a wall this close blocks it |
| `start_delay` | 0 s | Time after its spawn before HellForge moves it, such as a hellpod's landing |

Hover and fly modes come in a later SDK.

## Drive it from code

On the host's game thread, call `mover_input` every update while you drive. An input holds for half a second.

```c
HfMoverInput in = {sizeof in, 1.0f, 0.3f}; /* full ahead, a little left */
api->mover_input(mod, entity, &in);
```

`throttle` runs from -1 (full reverse) to 1 (full ahead). `steer` runs from -1 (full right) to 1 (full left). A mod's
input wins over `ai_drive` for as long as it holds, so a mod can make its own fight moves.

## Let the custom AI drive it

`ai_drive` gives a mover's units to the custom AI: Follow me and Hold position, routes round buildings, the leader's
trail, detours ([Orders for AI units](ai.md)). The AI only gives the inputs a driver would, so it never turns a vehicle
on the spot.

```c
HfAiDrive ai = {sizeof ai};
ai.start_order = HF_ORDER_FOLLOW;
ai.follow_distance = 3.0f;
api->ai_drive(mod, MY_SENTRY, &ai);
```

| Field | Default | What it does |
|---|---|---|
| `start_order` | none | `HF_ORDER_FOLLOW`: a new unit follows the nearest Helldiver |
| `range` | 60 m | It obeys a speaker this close |
| `follow_distance` | 6 m | Follow me keeps it this far from the speaker |
| `hold_radius` | 3 m | It may stray this far from its hold point |
| `nav_preset` | `HF_NAV_TROOPER` | The size its routes are found for |
| `arrive_distance` | 3 m | It slows down within this distance of where it goes |

## Read how it moves

`mover_state` answers on any thread. Use `speed` and `turning` to animate legs or wheels.

| Field | Meaning |
|---|---|
| `status` | `HF_MOVER_MOVING`, `HF_MOVER_BLOCKED`, `HF_MOVER_WAITING` (its start delay) or `HF_MOVER_REMOTE` |
| `driver` | `HF_DRIVER_MOD`, `HF_DRIVER_AI` or `HF_DRIVER_NONE` |
| `position`, `yaw` | Where its root is, and its heading (radians, counter-clockwise from +Y) |
| `speed`, `turning` | Metres a second along its heading, and radians a second to the left |
| `throttle`, `steer` | The inputs it moves by now |

## What HellForge does each update

1. It sets the speed from the throttle, no faster than `acceleration` allows.
2. It turns by the steer: at `turn_rate`, or no tighter than `turn_radius` for `HF_TURN_VEHICLE`.
3. It casts rays for the ground ahead. Helldivers, enemies and other units nearby are passed; rocks and walls are solid.
4. A step within `step_up` and `step_down` is taken. A walker tries 35, 70 and 105 degrees to either side first.
5. When no step works, `status` is `HF_MOVER_BLOCKED` and the speed drops to 0.
6. It writes the root's new position and heading, only when it moved or turned. Standing still, its parent (a hellpod) holds it.
