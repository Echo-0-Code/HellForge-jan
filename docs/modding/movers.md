# Movers

<!-- diataxis: how-to -->

SDK 35 lets HellForge move a unit itself: a sentry that walks, a cart that drives, any object your mod spawns. SDK 36
adds hover vehicles, brakes, water and drivetrains. You describe how it moves once with `add_mover`. A driver then
gives it a throttle and a steer: your code with `mover_input`, the custom AI with `ai_drive`, or a hover's seated
player. The types are in `hellforge_move.h`, which `hellforge_mod.h` includes.

## Contents

- [Before you call](#before-you-call)
- [Describe how it moves](#describe-how-it-moves)
- [Water](#water)
- [Drivetrains and your own drive code](#drivetrains-and-your-own-drive-code)
- [Hover vehicles](#hover-vehicles)
- [Drive it from code](#drive-it-from-code)
- [Let the custom AI drive it](#let-the-custom-ai-drive-it)
- [Read how it moves](#read-how-it-moves)
- [What HellForge does each update](#what-hellforge-does-each-update)

## Before you call

- Check `hf->size >= offsetof(HfApi, ai_drive) + sizeof(void*)` and set `"sdk": 36` in `hellforge-mod.json`.
- Call `add_mover` and `ai_drive` from `hellforge_mod_init`.
- Walkers and drivers run on the host. Their moves reach the other players as their root's moves do.
- The game's own physics no longer move a walker or driver. Leave out any movement code of your own for it.
- A mod built against SDK 35 keeps working: its shorter `HfMoverDef` gets SDK 35's behaviour, water ignored.

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
| `mode` | `HF_MOVE_WALK` | `HF_MOVE_WALK` steps to either side of a block. `HF_MOVE_DRIVE` goes only where it faces. `HF_MOVE_HOVER`: [Hover vehicles](#hover-vehicles) |
| `speed` | 4 m/s, hover 22 | Top speed forward |
| `reverse_speed` | half of `speed`, hover 0.7 × | Top speed backward |
| `acceleration` | 8 m/s², hover 3 | How fast it speeds up |
| `brake` | `acceleration` | How fast it slows down, stops or turns about |
| `turn` | `HF_TURN_SPOT` | `HF_TURN_SPOT` turns standing still. `HF_TURN_VEHICLE` turns only while it moves |
| `turn_rate` | 120 °/s | Its fastest turn |
| `turn_radius` | 4 m | `HF_TURN_VEHICLE`: its tightest circle |
| `step_up` | 0.5 m | The highest step it climbs |
| `step_down` | 1 m | The deepest drop it steps down |
| `reach` | 0.6 m | How far its body reaches ahead of its root: a wall this close blocks it |
| `start_delay` | 0 s | Time after its spawn before HellForge moves it, such as a hellpod's landing |
| `water`, `wade_depth` | wade, 0.8 m (drive 0.5) | [Water](#water) |
| `drivetrain`, `grip`, `drive` | none set | [Drivetrains](#drivetrains-and-your-own-drive-code) |

Fly mode comes in a later SDK.

## Water

A walker or driver looks at the water where each step lands. `wade_depth` is the deepest water it wades, or its draft
when it floats.

| `water` | What deeper water does |
|---|---|
| `HF_WATER_WADE` (default) | It stops at the edge: `status` is `HF_MOVER_BLOCKED` |
| `HF_WATER_BLOCK` | It stays out of all water |
| `HF_WATER_CROSS` | It floats across, `wade_depth` under the surface |
| `HF_WATER_IGNORE` | It walks along the bottom, as in SDK 35 |

The defaults are real fording depths: about 0.8 m for a person (mid-thigh) and 0.5 m for an SUV. A hover always crosses
water. `mover_state` gives `water_depth`, the water over the ground where it is.

## Drivetrains and your own drive code

`drivetrain` says which axles of an `HF_MOVE_DRIVE` mover push. With one set, its tyres' `grip` caps how hard it
speeds up and brakes, and slopes pull on it.

| `drivetrain` | Grip it pushes with |
|---|---|
| 0 (default) | Grip never limits it, as in SDK 35 |
| `HF_DRIVETRAIN_FWD` | The front axle's load. It falls uphill, as the load moves back |
| `HF_DRIVETRAIN_RWD` | The rear axle's load. It grows uphill |
| `HF_DRIVETRAIN_4WD` | All of its weight |
| `HF_DRIVETRAIN_NONE` | HellForge pushes nothing: your `drive` function does |

- `grip` is tyre friction: 0.7 (the default) for dirt and gravel, about 0.9 for tarmac, 0.3 for mud.
- Every wheel brakes, so braking is capped at `grip` × g whatever the drivetrain.
- The load moves back by about a quarter of (a/g + the slope's sine), a car's ratio of centre of mass height to wheelbase.
- A slope its grip can't climb holds it still. `mover_state` gives `grade` and the axles pushing now in `driven`.

With `HF_DRIVETRAIN_NONE`, HellForge calls `drive` on the host's game thread every update, before it moves the unit.
Fill `HfMoverPush` with `forward` and `side` in m/s² and `turn` in radians a second. HellForge keeps the ground, steps,
water and top speeds. Don't call `mover_*` from it.

```c
static void MyDrive(HfMod* mod, uint32_t entity, const HfMoverState* s, float dt, HfMoverPush* push, void* user)
{
    push->forward = s->throttle * 2.0f; /* a winch that pulls at 0.2 g */
    push->turn = s->steer * 0.5f;
}
```

## Hover vehicles

`HF_MOVE_HOVER` is for a vehicle with a physics body and seats. It stays the game's physics body, so it collides,
replicates and seats players as before. HellForge pushes its chassis on the machine that simulates it:

- It holds `hover_height` over the ground or water, by a spring at `hover_hz` and `hover_damping`.
- It levels its roll and pitch, and turns at the steer's rate. It turns on the spot unless `turn` is `HF_TURN_VEHICLE`.
- Thrust is `acceleration` along its level heading. Air drag tops it out at `speed` and `reverse_speed`.
- `brake` slows it against its whole slide, and holds it still with no throttle below 1.5 m/s.
- `side_drag` sets how long a sideways slide lasts.
- It takes hold once the surface is within twice `hover_height`, never while it hangs under the Pelican.

| Field | Default | What it does |
|---|---|---|
| `hover_height` | 1.5 m | Its root over the ground or water |
| `mass` | 3000 kg | Its physics body's mass, the M-102 FRV's. Pushes scale with it |
| `carried` | 0 | The share of its weight its physics data carries already |
| `hover_hz` | 1.2 Hz | How stiffly it holds its height |
| `hover_damping` | 0.7 | Its height and tilt springs' share of critical damping |
| `side_drag` | 0.5 /s | How fast a sideways slide dies |
| `drivetrain` | 0 | 0, or `HF_DRIVETRAIN_NONE` for your own `drive` push |

Make the physics data match:

- Set its `VRA` extraGravity z to `carried` × 9.81. If HellForge stops pushing, it then settles at the rest.
- Lower its wheels' friction to about 0.02, so they never drive it or hold it.
- Set `VRD` word 11 to 0. Touching wheels pin it below 2 m/s otherwise.
- Move its get-on joints down by the height it gains, so the prompts stay in reach.

The seated driver's W, S, A and D drive it, and the lever's reverse and neutral count. `mover_input` wins over the seat,
and the seat over `ai_drive` while its driver gave input in the last 2 s. `mover_state` gives `height` over the surface.

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
| `status` | `HF_MOVER_MOVING`, `HF_MOVER_BLOCKED`, `HF_MOVER_WAITING` (its start delay, or a hover not over a surface) or `HF_MOVER_REMOTE` |
| `driver` | `HF_DRIVER_MOD`, `HF_DRIVER_AI`, `HF_DRIVER_SEAT` (a hover's player) or `HF_DRIVER_NONE` |
| `position`, `yaw` | Where its root is, and its heading (radians, counter-clockwise from +Y) |
| `speed`, `turning` | Metres a second along its heading, and radians a second to the left |
| `throttle`, `steer` | The inputs it moves by now |
| `height`, `water_depth`, `grade`, `driven` | SDK 36: a hover's height, the water there, the slope ahead, the axles pushing |

## What HellForge does each update

1. It sets the speed from the throttle, no faster than `acceleration` and `brake` allow, and the drivetrain's grip.
2. It turns by the steer: at `turn_rate`, or no tighter than `turn_radius` for `HF_TURN_VEHICLE`.
3. It casts rays for the ground ahead. Helldivers, enemies and other units nearby are passed; rocks and walls are solid.
4. A step within `step_up` and `step_down`, and water its `water` setting allows, is taken. A walker tries 35, 70 and 105 degrees to either side first.
5. When no step works, `status` is `HF_MOVER_BLOCKED` and the speed drops to 0.
6. It writes the root's new position and heading, only when it moved or turned. Standing still, its parent (a hellpod) holds it.
