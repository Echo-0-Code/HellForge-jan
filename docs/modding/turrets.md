# Turrets

<!-- diataxis: how-to -->

SDK 37 sets up a turret with one call: how fast it turns, how far it may turn, how far it sees, how fast it fires and
who aims it. `add_turret` can also put the turret on another entity, such as a [mover](movers.md)'s back. The types
are in `hellforge_turret.h`, which `hellforge_mod.h` includes.

## Contents

- [Before you call](#before-you-call)
- [Set up a turret](#set-up-a-turret)
- [Put it on a host](#put-it-on-a-host)
- [Who aims it](#who-aims-it)
- [What HellForge writes](#what-hellforge-writes)

## Before you call

- Check `hf->size >= offsetof(HfApi, add_turret) + sizeof(void*)` and set `"sdk": 37` in `hellforge-mod.json`.
- Call `add_turret` from `hellforge_mod_init`, so your own entities get records of their own to change.
- Give it your own entity (`register_entity`). A vanilla entity's record is often shared, and a shared record is
  refused when the data loads.

## Set up a turret

```c
HfTurretDef gun = {sizeof gun};
gun.entity = MY_AUTOCANNON;
gun.yaw_speed = 90.0f;
gun.pitch_speed = 45.0f;
gun.limits = HF_TURRET_LIMIT_PITCH;
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
| `yaw_speed`, `pitch_speed` | Its fastest turn sideways and up or down, °/s |
| `pitch_coupling` | How much pitching slows while it turns sideways |
| `limits` | `HF_TURRET_LIMIT_YAW` and `HF_TURRET_LIMIT_PITCH`: which of the four angles below it sets |
| `yaw_min`, `yaw_max` | Traverse from its rest, -180 to 180°. `yaw_min` is its left limit |
| `pitch_min`, `pitch_max` | Elevation, -90 to 90° |
| `body_turn_speed` | A walker's body turn onto its target, °/s |
| `range`, `side_range`, `rear_range` | How far it sees ahead, to its sides and behind, metres |
| `aim_spring`, `aim_damping` | How hard its aim pulls on and how much its swing is held back |
| `weapon`, `fire_rate` | `HF_TURRET_WEAPON_PROJECTILE` or `HF_TURRET_WEAPON_BEAM`, and its rounds or pulses a minute |
| `rounds` | Rounds a magazine, at most 2047 |

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

| `ai` | `seat` | Who aims it |
|---|---|---|
| `HF_TURRET_AI_NATIVE` | 0 | The game's turret AI picks targets, aims and fires, as for a sentry |
| `HF_TURRET_AI_OFF` | a seat | The player in that seat. Seats aim turrets from a later SDK; until then the seat is kept |
| `HF_TURRET_AI_OFF` | 0 | Only your mod's code |

`HF_TURRET_AI_OFF` shuts the turret's sight, so it never picks a target. Its behaviour still runs, because the
behaviour also deploys it and feeds its gun. Leave the ranges at 0 with `HF_TURRET_AI_OFF`.

## What HellForge writes

`add_turret` queues [deferred edits](content.md) of the game's own records. The game's turret AI does all the aiming
and firing.

| Record | Fields |
|---|---|
| `Turret` | yaw and pitch speeds, coupling, limits |
| `Rotation` | `body_turn_speed` |
| `Targeting` | `aim_spring`, `aim_damping` |
| `SensorEye` | the three ranges, or 0 for `HF_TURRET_AI_OFF` |
| `ProjectileWeapon` or `BeamWeapon` | `fire_rate`, on `gun` |
| `WeaponMagazine` | `rounds`, on `gun` |
| the host's `Mount` | the slot and its node |

A setting whose record the entity lacks is refused when the data loads, and the mod's log names it. The other
settings still apply.
