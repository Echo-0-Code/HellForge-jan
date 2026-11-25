# Movers

<!-- diataxis: how-to -->

SDK 35 lets HellForge move a unit itself: a sentry that walks, a cart that drives, any object your mod spawns. SDK 36
adds hover vehicles, brakes, water and drivetrains. SDK 37 adds slopes, tilting, flying heights, boost, strafing and
sounds. SDK 43 adds helicopters, planes, boats, submarines and rail vehicles. You describe how it moves once with `add_mover`. A driver then
gives it a throttle and a steer: your code with `mover_input`, the custom AI with `ai_drive`, or the player in its
driver's seat, in any mode whose unit has one. The types are in `hellforge_move.h`, which `hellforge_mod.h` includes.

SDK 47 adds a helicopter's `thrust_max`, fixed weapons its driver fires, and damage zones.

## Contents

- [Before you call](#before-you-call)
- [Describe how it moves](#describe-how-it-moves)
- [Water](#water)
- [Drivetrains and your own drive code](#drivetrains-and-your-own-drive-code)
- [Hover vehicles](#hover-vehicles)
- [Helicopters and planes](#helicopters-and-planes)
- [Mouse steering](#mouse-steering)
- [Fixed weapons](#fixed-weapons)
- [Damage zones](#damage-zones)
- [Boats and submarines](#boats-and-submarines)
- [Rail vehicles](#rail-vehicles)
- [Slopes, tilt, boost and sounds](#slopes-tilt-boost-and-sounds)
- [Drive it from code](#drive-it-from-code)
- [Let the custom AI drive it](#let-the-custom-ai-drive-it)
- [Let it fight](#let-it-fight)
- [Read how it moves](#read-how-it-moves)
- [What HellForge does each update](#what-hellforge-does-each-update)

## Before you call

- Check `hf->size >= offsetof(HfApi, ai_drive) + sizeof(void*)` and set `"sdk": 39` in `hellforge-mod.json`. Fliers, vessels and rails need `"sdk": 43`, and `add_rail` its own `hf->size` check.
- Call `add_mover` and `ai_drive` from `hellforge_mod_init`.
- Walkers and drivers run on the host. HellForge sends their pose to the other players about 10 times a second, and their copies glide to it.
- The game's own physics no longer move a walker or driver. Leave out any movement code of your own for it.
- SDK 39 reordered `HfMoverDef`. Build movers against SDK 39's header or later; an older one is refused.

## Describe how it moves

```c
HfMoverDef walker = {sizeof walker};
walker.entity = MY_SENTRY;
walker.mode = HF_MOVE_WALK;
walker.speed = 3.5f;
walker.step_up = 0.6f;
api->add_mover(mod, &walker);
```

| Field | Default | What it does |
|---|---|---|
| `mode` | `HF_MOVE_WALK` | `HF_MOVE_WALK` steps to either side of a block. `HF_MOVE_DRIVE` (and its `_FWD`, `_RWD`, `_4WD`, and `HF_MOVE_TRACKS`) goes only where it faces. `HF_MOVE_HOVER`: [Hover vehicles](#hover-vehicles). `HF_MOVE_HELI`, `HF_MOVE_PLANE`: [Helicopters and planes](#helicopters-and-planes). `HF_MOVE_BOAT`, `HF_MOVE_SUB`: [Boats and submarines](#boats-and-submarines). `HF_MOVE_RAIL`: [Rail vehicles](#rail-vehicles) |
| `speed` | 4 m/s, hover 22, helicopter 40, plane 60 | Top speed forward, at most 40 m/s (a flier 120) |
| `reverse_speed` | 0: no reverse | Top speed backward in m/s. A negative value is a share of `speed`: -0.5 is half |
| `acceleration` | 8 m/s², hover 3 | How fast it speeds up |
| `brake` | `acceleration` | How fast it slows down, stops or turns about |
| `turn_rate` | 120 °/s | Its fastest turn |
| `turn_radius` | 0: on the spot | Its tightest circle. Above 0 it turns only while it moves, like wheels |
| `step_up` | 0.5 m | The highest step it climbs |
| `step_down` | 1 m | The deepest drop it steps down |
| `reach` | 0.6 m | How far its body reaches ahead of its root: a wall this close blocks it |
| `start_delay` | 0 s | Time after its spawn before HellForge moves it, such as a hellpod's landing |
| `water`, `wade_depth` | wade, 0.8 m (drive 0.5) | [Water](#water) |
| `grip`, `drive` | 0.7, none | [Drivetrains](#drivetrains-and-your-own-drive-code) |
| `max_slope`, `tilt_limit` | from `step_up`, 0 (upright) | [Slopes, tilt, boost and sounds](#slopes-tilt-boost-and-sounds) |
| `boost_speed`, `boost_time`, `strafe_speed`, `sounds` | none, 3 s, half of `speed`, none | The same section |
| `hover` | | [Hover vehicles](#hover-vehicles) |
| `stall_speed`, `bank_max` | 0.45 × `speed`, 40° | [Helicopters and planes](#helicopters-and-planes) |
| `dive_depth` | 3 m | [Boats and submarines](#boats-and-submarines) |
| `rail` | 0: the nearest | [Rail vehicles](#rail-vehicles) |
| `mouse_steer` | 0: the keys | [Mouse steering](#mouse-steering) |

## Water

A walker or driver looks at the water where each step lands. `wade_depth` is the deepest water it wades, or its draft
when it floats.

| `water` | What deeper water does |
|---|---|
| `HF_WATER_WADE` (default) | It wades to `wade_depth` and stops at deeper water: `status` is `HF_MOVER_BLOCKED` |
| `HF_WATER_DRY` | It stays out of all water |
| `HF_WATER_FLOAT` | It floats across, `wade_depth` (its draft) under the surface |
| `HF_WATER_SINK` | It goes along the bottom, however deep |

The defaults are real fording depths: about 0.8 m for a person (mid-thigh) and 0.5 m for an SUV. A hover always crosses
water. `mover_state` gives `water_depth`, the water over the ground where it is.

## Drivetrains and your own drive code

The drive modes say which axles push. With one of these four, its tyres' `grip` caps how hard it speeds up and
brakes, and slopes pull on it.

| `mode` | Grip it pushes with |
|---|---|
| `HF_MOVE_DRIVE` | Grip never limits it, as in SDK 35 |
| `HF_MOVE_DRIVE_FWD` | The front axle's load. It falls uphill, as the load moves back |
| `HF_MOVE_DRIVE_RWD` | The rear axle's load. It grows uphill |
| `HF_MOVE_DRIVE_4WD` | All of its weight |
| `HF_MOVE_TRACKS` | All of its weight, with `grip` 1. It pivots on the spot, and a hard turn slows it: half its speed at full steer |

- `grip` is tyre friction: 0.7 (the default) for dirt and gravel, about 0.9 for tarmac, 0.3 for mud.
- Every wheel brakes, so braking is capped at `grip` × g whatever the axles.
- The load moves back by about a quarter of (a/g + the slope's sine), a car's ratio of centre of mass height to wheelbase.
- A slope its grip can't climb holds it still. `mover_state` gives `grade` and the axles pushing now in `driven`.

In any mode, a `drive` function replaces HellForge's push from the inputs. HellForge calls it on the host's game thread
every update, before it moves the unit. Fill `HfMoverPush` with `forward` and `side` in m/s² and `turn` in radians a second. HellForge keeps the ground, steps,
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

- It holds `hover.height` over the ground or water, by a spring at `hover.stiffness` and `hover.damping`.
- It levels its roll and pitch, and turns at the steer's rate. It turns on the spot unless `turn_radius` is above 0.
- Thrust is `acceleration` along its level heading. Air drag tops it out at `speed` and `reverse_speed` (-0.7 is the tested craft's).
- `brake` slows it against its whole slide, and holds it still with no throttle below 1.5 m/s.
- `hover.side_drag` sets how long a sideways slide lasts.
- It takes hold once the surface is within twice `hover.height`, never while it hangs under the Pelican.

| `hover.` field | Default | What it does |
|---|---|---|
| `height` | 1.5 m | Its root over the ground or water |
| `height_min`, `height_max` | `height` | The heights the `lift` input moves it between, up to 25 m |
| `climb_rate` | 2 m/s | How fast full `lift` raises or lowers it |
| `carried` | 0 | The share of its weight its physics data carries already |
| `mass` | 3000 kg | Its physics body's mass, the M-102 FRV's. Pushes scale with it |
| `stiffness` | 1.2 Hz | How stiffly it holds its height and level |
| `damping` | 0.7 | Its height and tilt springs' share of critical damping |
| `side_drag` | 0.5 /s | How fast a sideways slide dies |

A hover's `strafe_speed` defaults to 5 m/s, and its `drive` function, if any, gives forward and sideways pushes and a turn.

Make the physics data match:

- Set its `VRA` extraGravity z to `hover.carried` × 9.81. If HellForge stops pushing, it then settles at the rest.
- Lower its wheels' friction to about 0.02, so they never drive it or hold it.
- Set `VRD` word 11 to 0. Touching wheels pin it below 2 m/s otherwise.
- Move its get-on joints down by the height it gains, so the prompts stay in reach.

The seated driver's W, S, A and D drive it, and the lever's reverse and neutral count. `mover_input` wins over the seat,
and the seat over `ai_drive` while its driver gave input in the last 2 s. `mover_state` gives `height` over the surface.

## Helicopters and planes

`HF_MOVE_HELI` and `HF_MOVE_PLANE` are fliers. Like a hover, a flier is the game's physics body with seats, and HellForge
pushes its chassis. It uses the same `hover` block, with heights up to 500 m.

```c
HfMoverDef heli = {sizeof heli};
heli.entity = MY_GUNSHIP;
heli.mode = HF_MOVE_HELI;
heli.reverse_speed = -0.3f;
heli.hover.height = 12.0f;
heli.hover.height_max = 60.0f;
api->add_mover(mod, &heli);
```

Both wait until they sit on the ground, such as after a Pelican drop.

How a helicopter climbs:

- `lift` moves its collective: the altitude where its thrust equals its weight moves at `hover.climb_rate`. Let go and
  that altitude stays, so it settles there.
- Altitude counts from the ground where it first sat down, not from the ground under it. Hills don't lift it: fly
  around them or climb over them.
- Thrust thins with altitude like air, which slows the climb near `hover.height_max` (its ceiling).
- `thrust_max` (SDK 47) is its rotor's most thrust over its weight, 1.05 to 3 (default 1.3). A higher one climbs
  harder at full lift.
- `hover.damping` sets how fast it settles. `hover.height` and `stiffness` don't apply.
- When nobody has driven it for 3 s, it lowers the collective at half `climb_rate` and lands.

How a plane takes off and lands:

- It holds `hover.height_min` on its gear. Once anyone drives it, it climbs to `hover.height` over the ground at
  `hover.climb_rate`. The `lift` input moves it up to `hover.height_max`.
- Aloft it holds an altitude, counted from where it first sat down, as a helicopter does. Hills don't pitch it up and
  down. The ground under it stays its floor.
- When nobody has driven it for 3 s, it lands: it sinks to `height_min`.

A helicopter hovers in place and turns on the spot. Its thrust tilts toward the way it speeds up, up to 30° or
`tilt_limit`. `strafe` slides it sideways.

A plane needs airspeed:

- Below `stall_speed` it can't climb, and its wings carry less as it slows: lift grows with speed squared.
- On its gear it rolls and steers like a car, with an 8 m `turn_radius`. It takes off once it passes `stall_speed`.
- Aloft, steer banks it up to `bank_max`, and the bank sets the turn: g × tan(bank) / speed.
- Throttle below 0 brakes: air brakes aloft, wheel brakes on the ground. Rolling slower than 1.5 m/s, it parks.

| Field | Helicopter | Plane |
|---|---|---|
| `speed`, `acceleration` | 40 m/s, 5 m/s² | 60 m/s, 4 m/s² |
| `turn_rate`, `strafe_speed` | 45 °/s, 10 m/s | 30 °/s |
| `hover.height`, `height_min`, `height_max` | 10 m, 1 m, 4 × `height` | 40 m, 1 m, 4 × `height` |
| `hover.climb_rate`, `hover.mass`, `hover.side_drag` | 5 m/s, 2500 kg, 0.8 /s | 8 m/s, 1100 kg, 3 /s |

Set its physics data as for a hover: `VRA` extraGravity at `hover.carried`, wheels at low friction, and `VRD` word 11 at 0.
A seated driver gives throttle and steer, and `lift` with Space (up) and Control (down).

## Mouse steering

SDK 48's `mouse_steer` 1 lets a seated driver fly with the mouse. The mover turns and climbs toward where the camera looks.

- A plane aloft banks into the turn, up to `bank_max`, rolling at most 90° a second. On its gear, or below
  `stall_speed`, it steers like a car.
- A flier's `lift` follows the camera's pitch, eased in: a plane climbs or dives along it, a helicopter moves its
  collective. About 3° either side of level holds height.
- A/D steer by hand while held, such as a roll. Space and Control still move `lift`.
- W/S move a throttle lever that stays where it is left. S with the lever closed brakes, as throttle below 0 does.
- It counts as driven while the lever is open, so a pilot can fly with the mouse alone.
- Only this machine's driver steers by its camera. Another machine's driver drives with the keys.

`view_pose` (docs/modding/world.md) reads the same camera, for a HUD that marks where the mover flies.

## Fixed weapons

SDK 47's `add_fixed_gun` gives a mover a gun or rocket pod locked to its nose. Its driver aims by pointing the vehicle.

```c
HfFixedGunDef gun = {sizeof gun};
gun.entity = MY_GUNSHIP;
gun.projectile = HF_PROJECTILE_MG43_MACHINE_GUN; /* or add_projectile's type */
gun.rounds = 500;
gun.fire_rate = 900;
gun.muzzle[0] = -0.6f; gun.muzzle[1] = 1.4f; gun.muzzle[2] = 0.7f;
gun.seat[1] = 0.2f; gun.seat[2] = 1.0f;
uint32_t id = api->add_fixed_gun(mod, &gun);
```

- It fires on the host while that machine's Helldiver sits within 1.2 m of `seat` and holds `trigger`: the fire
  button (left mouse) or the aim button (right mouse).
- Each round is a `spawn_projectile` shot, credited to that Helldiver.
- `burst` fires that many rounds a press; 0 fires while held. `spread`, `converge` and `recoil` shape the shots.
- Points are metres in the mover's frame: x right, y forward, z up.
- `fixed_gun_state` reads a unit's rounds. `fixed_gun_refill` adds rounds, 0 for full.

## Damage zones

SDK 47's `add_damage_zone` puts a sphere on a mover that takes the hits landing in it.

| Field | Default | What it does |
|---|---|---|
| `effect` | `HF_ZONE_PLAIN` | `_ENGINE`: broken, it comes down at `sink`. `_CONTROL`: it also spins at `spin` |
| `center`, `radius` | 0.5 m | Where the sphere is, in the mover's frame |
| `health` | 500 | Damage, before armour, it takes before it breaks |
| `scale` | 1 | × the damage the vehicle itself takes from a hit in it; negative: none |
| `sink`, `spin` | 5 m/s, 60 °/s | The broken zone's descent and spin |

- A broken engine or control zone lowers a helicopter's collective and holds its fall to `sink`, as autorotation
  does. A plane glides down.
- Zones see a hit before `HF_ON_DAMAGE_PRE` listeners, so listeners get the scaled amount.
- `zone_state` reads a zone. `repair_zones` makes one, or all, whole again.

A zone with `strike_damage` is a spinning disc, such as an open rotor, swept against the scenery every 0.1 s:

- The ground and static objects such as trees strike it: each new contact, and each second one lasts, takes
  `strike_damage` of its health.
- A unit it touches, such as a building, is bumped when `strike_speed` is set. Slower than `strike_speed` toward it,
  the mover is pushed back unharmed. Faster, the zone breaks and `strike_explosion` goes off where it touched.
- `disc` is the disc's axis in the mover's frame, and `radius` its radius. Hits land in it only within 0.2 m of the
  disc's plane, so a big rotor doesn't take the hits on the cabin under it.

## Boats and submarines

`HF_MOVE_BOAT` and `HF_MOVE_SUB` are vessels. A vessel is the game's physics body with seats, pushed like a hover.

- HellForge's buoyancy holds its root `wade_depth` under the water's surface: its draft. The spring is `hover.stiffness`.
- It is pushed and braked like a hover, and steered by a rudder: it turns only while it moves, no tighter than `turn_radius`.
- `hover.side_drag` is its keel: how fast a sideways slide dies.
- In water shallower than its draft plus 0.2 m it sits aground, and HellForge lets go until it floats again.
- A submarine's `lift` input dives it at `hover.climb_rate`, down to `dive_depth` and never closer than 0.2 m to the bottom.

The game draws nothing under water and its maps have no open sea, so a dive stays near the surface. Spawn a vessel on
the water: one dropped on land sits aground.

| Field | Boat | Submarine |
|---|---|---|
| `speed`, `acceleration` | 12 m/s, 1.5 m/s² | 7 m/s, 0.8 m/s² |
| `turn_rate`, `turn_radius` | 20 °/s, 12 m | 12 °/s, 15 m |
| `wade_depth` (draft) | 0.5 m | 1.5 m |
| `hover.mass`, `hover.side_drag`, `hover.stiffness` | 4000 kg, 2 /s, 0.6 Hz | 8000 kg, 2 /s, 0.6 Hz |
| `hover.climb_rate` | | 0.5 m/s (its dive rate) |

`mover_state` gives `height` as its root over the surface, negative under it, and `water_depth` under it.

## Rail vehicles

`HF_MOVE_RAIL` locks a unit to a rail. Give the line first with `add_rail`, its points at track height in world metres:

```c
static const float kLine[] = {0, 0, 10, 0, 50, 10, 40, 90, 12, 120, 100, 12};
HfRailDef line = {sizeof line, 4, kLine, 0};
const uint32_t rail = api->add_rail(mod, &line);   /* 0: refused, the log says why */

HfMoverDef tram = {sizeof tram};
tram.entity = MY_TRAM;
tram.mode = HF_MOVE_RAIL;
tram.rail = rail;
tram.reverse_speed = -1.0f;
api->add_mover(mod, &tram);
```

- A rail has 2 to 4096 points, each at least 5 cm from the one before. `loop` 1 joins its last point to its first.
- A rail mover starts on its `rail`, or with 0 on the nearest one, at most 20 m from where it spawns.
- It faces the way along the line nearest its heading. Throttle moves it that way, and back at `reverse_speed`.
- Steer does nothing. Its heading follows the track, and with a `tilt_limit` its pitch follows the grade.
- At a line's end it stops, as at a buffer, and `status` is `HF_MOVER_BLOCKED`.
- It is moved by its root like a driver, so it runs on the host. It ploughs through units and walls on its way.
- A vehicle copy's physics body is pushed along the line instead, and its wheels keep its height.

Its defaults are light rail's: 15 m/s, and 1 m/s² to speed up and brake. A mod builds its own track and lays it along
the line. Lines from the map's own routes come in a later SDK.

## Slopes, tilt, boost and sounds

- `max_slope` is the steepest incline a walker or driver climbs. A step is a sudden rise and a slope a long one, so the
  incline is read over at least 0.5 m and averaged over 2 m of travel. The default rises `step_up` in 1 m: 27° for 0.5 m steps.
- `tilt_limit` above 0 pitches and rolls a walker or driver with the ground under it, up to that angle. A hover then
  rights itself to the ground's slope instead of level, and harder once it tilts past it. 0 keeps it upright.
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

A unit with a driver's seat, such as a vehicle copy, is also driven from it in any mode. A walker, driver, rail or
tracked mover reads the seat on the host, so there it is the host's own player who drives it.

SDK 37 adds three inputs. `mover_input` gives all three. A seated driver gives `lift` with Space (up) and Control
(down), on a craft its seat drove in the last 30 s.

| Input | Range | What it does |
|---|---|---|
| `strafe` | -1 right .. 1 left | Slides a walker or hover sideways |
| `lift` | -1 down .. 1 up | Moves a hover's or flier's height, or a sub's depth, at `hover.climb_rate` |
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

On the other players' machines, a walker's or driver's `status` is `HF_MOVER_REMOTE`. Its other fields are the host's, so
the same animation code works there.

| Field | Meaning |
|---|---|
| `status` | `HF_MOVER_MOVING`, `HF_MOVER_BLOCKED`, `HF_MOVER_WAITING` (its start delay, or a hover not over a surface) or `HF_MOVER_REMOTE` |
| `driver` | `HF_DRIVER_MOD`, `HF_DRIVER_AI`, `HF_DRIVER_SEAT` (the player in its driver's seat) or `HF_DRIVER_NONE` |
| `position`, `yaw` | Where its root is, and its heading (radians, counter-clockwise from +Y) |
| `speed`, `turning` | Metres a second along its heading, and radians a second to the left |
| `throttle`, `steer` | The inputs it moves by now |
| `height`, `water_depth`, `grade`, `driven` | SDK 36: a hover's height, the water there, the slope ahead, the axles pushing |
| `pitch`, `roll`, `boost_left`, `boosting`, `strafe`, `lift` | SDK 37: its tilt in radians, its boost, the two new inputs |

## What HellForge does each update

1. It sets the speed from the throttle, no faster than `acceleration` and `brake` allow, and its tyres' grip.
2. It turns by the steer: at `turn_rate`, or no tighter than `turn_radius` when it has one.
3. It casts rays for the ground ahead. Helldivers, enemies and other units nearby are passed; rocks and walls are solid.
4. A step within `step_up` and `step_down`, and water its `water` setting allows, is taken. A walker tries 35, 70 and 105 degrees to either side first.
5. When no step works, `status` is `HF_MOVER_BLOCKED` and the speed drops to 0.
6. It writes the root's new position and heading, only when it moved or turned. Standing still, its parent (a hellpod) holds it.
7. When the game moves it between its own moves, such as a Pelican drop or a push, it goes on from where the game shows it.
8. A unit with a vehicle's physics body puts its root back where the body is, so written moves don't stick. Once that happens twice while it drives, HellForge pushes the body instead, with `hover.mass` as its mass. Ground, walls and height stay the body's own.
