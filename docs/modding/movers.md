# Movers

<!-- diataxis: how-to -->

SDK 35 lets HellForge move a unit itself: a sentry that walks, a cart that drives, any object your mod spawns. SDK 36
adds hover vehicles, brakes, water and drivetrains. SDK 37 adds slopes, tilting, flying heights, boost, strafing and
sounds. You describe how it moves once with `add_mover`. A driver then
gives it a throttle and a steer: your code with `mover_input`, the custom AI with `ai_drive`, or a hover's seated
player. The types are in `hellforge_move.h`, which `hellforge_mod.h` includes.

## Contents

- [Before you call](#before-you-call)
- [Describe how it moves](#describe-how-it-moves)
- [Water](#water)
- [Drivetrains and your own drive code](#drivetrains-and-your-own-drive-code)
- [Hover vehicles](#hover-vehicles)
- [Slopes, tilt, boost and sounds](#slopes-tilt-boost-and-sounds)
- [Drive it from code](#drive-it-from-code)
- [Let the custom AI drive it](#let-the-custom-ai-drive-it)
- [Let it fight](#let-it-fight)
- [Read how it moves](#read-how-it-moves)
- [What HellForge does each update](#what-hellforge-does-each-update)

## Before you call

- Check `hf->size >= offsetof(HfApi, ai_drive) + sizeof(void*)` and set `"sdk": 37` in `hellforge-mod.json`.
- Call `add_mover` and `ai_drive` from `hellforge_mod_init`.
- Walkers and drivers run on the host. Their moves reach the other players as their root's moves do.
- The game's own physics no longer move a walker or driver. Leave out any movement code of your own for it.
- A mod built against SDK 35 or 36 keeps working: its shorter `HfMoverDef` keeps that SDK's behaviour (SDK 35 ignores
  water; neither has a slope limit).

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
| `max_slope`, `tilt_limit`, `align_to_ground` | from `step_up`, 30°, off | [Slopes, tilt, boost and sounds](#slopes-tilt-boost-and-sounds) |
| `boost_speed`, `boost_time`, `strafe_speed`, `sounds` | none, 3 s, `reverse_speed`, none | The same section |

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

FWD, RWD and 4WD are for `HF_MOVE_DRIVE` only; a walker or hover refuses them. `HF_DRIVETRAIN_NONE` works in every mode.
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
| `fly_height_min`, `fly_height_max` | `hover_height` | The heights the `lift` input moves it between, up to 25 m |
| `climb_rate` | 2 m/s | How fast full `lift` raises or lowers it |
| `strafe_speed` | 5 m/s | Its sideways top speed at full `strafe` |

Make the physics data match:

- Set its `VRA` extraGravity z to `carried` × 9.81. If HellForge stops pushing, it then settles at the rest.
- Lower its wheels' friction to about 0.02, so they never drive it or hold it.
- Set `VRD` word 11 to 0. Touching wheels pin it below 2 m/s otherwise.
- Move its get-on joints down by the height it gains, so the prompts stay in reach.

The seated driver's W, S, A and D drive it, and the lever's reverse and neutral count. `mover_input` wins over the seat,
and the seat over `ai_drive` while its driver gave input in the last 2 s. `mover_state` gives `height` over the surface.

## Slopes, tilt, boost and sounds

- `max_slope` is the steepest incline a walker or driver climbs. A step is a sudden rise and a slope a long one, so the
  slope is measured over several steps. The default rises `step_up` in 1 m: 27° for 0.5 m steps.
- `align_to_ground` 1 pitches and rolls a walker or driver with the ground under it, up to `tilt_limit`. A hover
  rights itself to the ground's slope instead of level, and harder once it tilts past `tilt_limit`.
- `boost_speed` is its top speed while the `boost` input holds. A full boost lasts `boost_time` and refills in twice
  that. A hover's fans push harder to reach it.
- `strafe_speed` is a walker's or hover's sideways top speed at full `strafe`. A driver doesn't strafe.

`sounds` holds one Wwise event per change of state, posted on its unit (`hf_wwise_id`). 0 is none.

| Slot | Posted when |
|---|---|
| `start`, then `moving` | It sets off. `moving` can start a loop |
| `stop`, then `idle` | It comes to rest. `idle` also plays when it starts |
| `boost` | A boost begins |
| `blocked` | A step, wall, slope or water stops it |

A loop that `moving` or `idle` starts is your sound bank's to end, on the next `start` or `stop` event.

## Drive it from code

On the host's game thread, call `mover_input` every update while you drive. An input holds for half a second.

```c
HfMoverInput in = {sizeof in, 1.0f, 0.3f}; /* full ahead, a little left */
api->mover_input(mod, entity, &in);
```

`throttle` runs from -1 (full reverse) to 1 (full ahead). `steer` runs from -1 (full right) to 1 (full left). A mod's
input wins over `ai_drive` for as long as it holds, so a mod can make its own fight moves.

SDK 37 adds three inputs. Only `mover_input` gives them; a seated player can't yet.

| Input | Range | What it does |
|---|---|---|
| `strafe` | -1 right .. 1 left | Slides a walker or hover sideways |
| `lift` | -1 down .. 1 up | Moves a hover's height at `climb_rate` |
| `boost` | 0 or 1 | Boosts while its boost lasts |

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

## Let it fight

From SDK 37, `ai_drive` can also move a unit to fight. Its turret aims and fires on the game's AI ([Turrets](turrets.md));
these fields only choose where the unit stands. A unit fights only when `engage_range` or `retreat_health` is set.

```c
ai.engage_range = 40.0f;
ai.keep_distance = 15.0f;
ai.retreat_health = 0.3f;
ai.retreat_style = HF_RETREAT_FIGHT;
```

| Field | Default | What it does |
|---|---|---|
| `engage_range` | 0 | It drives onto enemies this close. 0: it fires only from where its orders take it |
| `keep_distance` | 12 m | Engaging, it stands this far from its target |
| `leash` | 25 m | Fighting, it strays at most this far from whom it follows or its hold point |
| `react_time` | 0.5 s | An enemy stays in range this long before it moves to engage |
| `retreat_health` | 0 | Below this share of its health it retreats from enemies near it. 0: it never retreats |
| `retreat_style` | `HF_RETREAT_RUN` | `HF_RETREAT_RUN` turns and drives back. `HF_RETREAT_FIGHT` backs off facing the enemy |
| `owner` | `HF_OWNER_ANY` | `HF_OWNER_CALLER`: only the Helldiver nearest it when it lands. `HF_OWNER_CODE`: only `ai_order` |
| `spacing` | 0 | Standing, it keeps this far from other friendly units. Helldivers are left out |

`range` is how near a speaker must be for any order to reach it. A unit retreats from enemies within `engage_range`,
or within 30 m when that is 0. Once the fight is over it goes back to its order.

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
| `pitch`, `roll`, `boost_left`, `boosting`, `strafe`, `lift` | SDK 37: its tilt in radians, its boost, the two new inputs |

## What HellForge does each update

1. It sets the speed from the throttle, no faster than `acceleration` and `brake` allow, and the drivetrain's grip.
2. It turns by the steer: at `turn_rate`, or no tighter than `turn_radius` for `HF_TURN_VEHICLE`.
3. It casts rays for the ground ahead. Helldivers, enemies and other units nearby are passed; rocks and walls are solid.
4. A step within `step_up` and `step_down`, and water its `water` setting allows, is taken. A walker tries 35, 70 and 105 degrees to either side first.
5. When no step works, `status` is `HF_MOVER_BLOCKED` and the speed drops to 0.
6. It writes the root's new position and heading, only when it moved or turned. Standing still, its parent (a hellpod) holds it.
