/* HellForge mod SDK 37: turrets on movers and objects (docs/modding/turrets.md). Included by hellforge_move.h.
 * add_turret sets up a turret entity once: how fast it turns, how far it may turn, how far it sees, how fast it fires,
 * and who aims it. It may also put the turret on another entity's mount slot, such as a mover's back. HellForge writes
 * the game's own records (Turret, Rotation, Targeting, SensorEye, the weapon's), so the game's turret AI aims and fires
 * it as it does a sentry. */
#ifndef HELLFORGE_TURRET_H
#define HELLFORGE_TURRET_H

#include <stdint.h>

/* HfTurretDef.ai */
#define HF_TURRET_AI_NATIVE 0u /* the game's turret AI picks targets, aims and fires (a sentry's) */
#define HF_TURRET_AI_OFF 1u    /* its sight is shut, so it never picks a target: a seat or the mod aims it */

/* HfTurretDef.limits: which traverse limits it sets (0 is a real angle, so a flag says the limit is given) */
#define HF_TURRET_LIMIT_YAW 1u
#define HF_TURRET_LIMIT_PITCH 2u

/* HfTurretDef.weapon: the weapon record fire_rate goes to */
#define HF_TURRET_WEAPON_PROJECTILE 0u /* ProjectileWeapon: guns, cannons, launchers */
#define HF_TURRET_WEAPON_BEAM 1u       /* BeamWeapon: lasers */

/* One turret entity (add_turret). Zero-initialize it and set size and entity; every other zero field keeps the game's
 * value. Angles are degrees from the turret's rest, as its TurretComponentData has them (yaw_min is its left limit). */
typedef struct HfTurretDef
{
    uint32_t size;          /* sizeof(HfTurretDef) */
    uint32_t ai;            /* HF_TURRET_AI_NATIVE or HF_TURRET_AI_OFF */
    uint64_t entity;        /* the entity that aims: a sentry, a walker, a turret a vehicle carries */
    uint64_t gun;           /* the entity whose weapon fires, when it is not `entity` (a walker's mounted gun), or 0 */
    uint64_t host;          /* an entity to carry it (a mover, a vehicle, an object), or 0 */
    uint32_t mount_slot;    /* host: the MountComponentData slot it goes in, 0 .. 4 */
    uint32_t attach_node;   /* host: the node it attaches to (a joint's name hash), or 0 for the slot's own */
    float yaw_speed;        /* its fastest sideways turn, degrees a second */
    float pitch_speed;      /* its fastest turn up and down, degrees a second */
    float pitch_coupling;   /* how much pitching slows while it turns sideways (an exponent; the game's own if 0) */
    uint32_t limits;        /* HF_TURRET_LIMIT_ flags: which of the four angles below it sets */
    float yaw_min, yaw_max;     /* traverse, -180 .. 180 */
    float pitch_min, pitch_max; /* elevation, -90 .. 90 */
    float body_turn_speed;  /* a walker's body turn onto its target (RotationComponentData), degrees a second */
    float range;            /* how far it sees ahead, metres */
    float side_range;       /* how far it sees to its sides, metres */
    float rear_range;       /* how far it sees behind it, metres */
    float aim_spring;       /* how hard its aim pulls onto a target (TargetingComponentData; vanilla sentries 10) */
    float aim_damping;      /* how much its aim's swing is held back (vanilla 1; under-damped aim never settles to fire) */
    uint32_t weapon;        /* HF_TURRET_WEAPON_PROJECTILE or HF_TURRET_WEAPON_BEAM */
    float fire_rate;        /* rounds (or beam pulses) a minute */
    uint32_t rounds;        /* rounds a magazine, at most 2047 */
    uint32_t seat;          /* the host's seat that aims it, counted from 1, or 0 for none; seat control comes in a later SDK */
} HfTurretDef;

#endif
