# Turrets

<!-- diataxis: how-to -->

SDK 39 sets up a turret with one call: how fast it turns, how far it may turn, how far it sees, what and how fast it
fires and who aims it. `add_turret` can also put the turret on another entity, such as a [mover](movers.md)'s back. The
types are in `hellforge_turret.h`, which `hellforge_mod.h` includes.

## Contents

- [Before you call](#before-you-call)
- [Set up a turret](#set-up-a-turret)
- [Put it on a host](#put-it-on-a-host)
- [Who aims it](#who-aims-it)
- [What HellForge writes](#what-hellforge-writes)
- [Change it live](#change-it-live)

## Before you call

- Check `hf->size >= offsetof(HfApi, add_turret) + sizeof(void*)` and set `"sdk": 39` in `hellforge-mod.json`.
- Build with the SDK 39 header or later. An SDK 37 `HfTurretDef` is refused.
- `face` needs `"sdk": 41`; an SDK 39 or 40 `HfTurretDef` keeps the game's facing.
- Call `add_turret` from `hellforge_mod_init`, so your own entities get records of their own to change.
- Give it your own entity (`register_entity`). A vanilla entity's record is often shared, and a shared record is
  refused when the data loads.

## Set up a turret

```c
HfTurretDef gun = {sizeof gun};
gun.entity = MY_AUTOCANNON;
gun.aimer = HF_AIM_AI;
gun.yaw_speed = 90.0f;
gun.pitch_speed = 45.0f;
gun.pitch_min = -10.0f;
gun.pitch_max = 60.0f;
gun.range = 80.0f;
gun.fire_rate = 150.0f;
api->add_turret(mod, &gun);
```

Every field left at 0 keeps the game's value.

| Field | What it does |
|---|---|
| `entity` | The entity that aims: a sentry, a walker, a turret a vehicle carries |
| `gun` | The entity whose weapon fires, when it is not `entity`, such as a walker's mounted gun |
| `aimer` | Who aims it, see [Who aims it](#who-aims-it) |
| `yaw_speed`, `pitch_speed` | Its fastest turn sideways and up or down, °/s |
| `pitch_coupling` | How much pitching slows while it turns sideways |
| `yaw_min`, `yaw_max` | Traverse from its rest, -180 to 180°. `yaw_min` is its left limit. Both 0 keep the game's |
| `pitch_min`, `pitch_max` | Elevation, -90 to 90°. Both 0 keep the game's |
| `body_turn_speed`, `body_turn_accel`, `body_turn_decel` | A walker's body turn onto its target: °/s, and how fast it speeds up and slows |
| `range` | How far it sees, metres, all round |
| `side_range`, `rear_range` | A shorter sight to its sides or behind, or 0 for `range` |
| `aim_spring`, `aim_damping` | How hard its aim pulls on and how much its swing is held back |
| `projectile` | What it fires: a vanilla `HF_PROJECTILE_` type or one from `add_projectile`. 0 keeps its own |
| `fire_rate` | Rounds or beam pulses a minute |
| `rounds` | Rounds a magazine, at most 2047 |
| `face` | SDK 41: `HF_FACE_AWAY` turns it away from the Helldiver who called it in, once it lands. Not with a `host` |

A thrown sentry lands facing the world's +Y, whichever way it was thrown. With yaw limits that guards the same compass
direction every time, so set `face` to `HF_FACE_AWAY`. A turret on a host turns with its host.

A projectile needs a gun with a projectile weapon; a beam gun keeps its beam. The mod's log names a setting the gun
can't take.

An aim that swings too freely never settles inside the fire cone, so the turret never fires. The vanilla aim is
spring 10, damping 1. Spring 25 and damping 10 settle a walker's gun.

## Put it on a host

Set `host` to the entity that carries it, and `mount_slot` to one of its five `MountComponentData` slots. The game
spawns the turret as the host's child in that slot. `attach_node` names the joint it sits on, by the joint name's hash.

```c
gun.host = MY_STRIDER;
gun.mount_slot = 1;
gun.attach_node = MY_BACK_JOINT;
```

## Who aims it

| `aimer` | `seat` | Who aims it |
|---|---|---|
| `HF_AIM_AI` | 0 | The game's turret AI picks targets, aims and fires, as for a sentry |
| `HF_AIM_PLAYER` | a seat | Only the player in that seat of the host, counted from 1 |
| `HF_AIM_BOTH` | a seat | The player in that seat, and the AI while the seat is empty |
| `HF_AIM_CUSTOM` | 0 | Only your mod's code |

`HF_AIM_PLAYER` and `HF_AIM_CUSTOM` shut the turret's sight, so it never picks a target. Leave the ranges at 0 with
them. Its behaviour still runs, because the behaviour also deploys it and feeds its gun.

Seats aim turrets from a later SDK. Until then the seat is kept, and `HF_AIM_BOTH` aims as `HF_AIM_AI`.

## What HellForge writes

`add_turret` queues [deferred edits](content.md) of the game's own records. The game's turret AI does all the aiming
and firing.

| Record | Fields |
|---|---|
| `Turret` | yaw and pitch speeds, coupling, limits |
| `Rotation` | the body turn's speed, acceleration and deceleration |
| `Targeting` | `aim_spring`, `aim_damping` |
| `SensorEye` | the three ranges, or 0 for `HF_AIM_PLAYER` and `HF_AIM_CUSTOM` |
| `ProjectileWeapon` or `BeamWeapon` | `fire_rate`, and `projectile` on a projectile weapon, on `gun` |
| `WeaponMagazine` | `rounds`, on `gun` |
| the host's `Mount` | the slot and its node |

In a mission, the mod's log has a line for each spawn of the turret and, with `HF_FACE_AWAY`, where it landed and
which way it faces.

A setting whose record the entity lacks is refused when the data loads, and the mod's log names it. The other
settings still apply.

## Change it live

SDK 54: call `add_turret` again for the same entity, on the game thread, to replace its settings. HellForge makes the
edits above again, except the mount.

- `host`, `mount_slot` and `attach_node` stay; a call that changes them is refused.
- A 0 keeps the value set before, not the game's.
- Only the mod that added the turret may change it. Another thread is refused.
