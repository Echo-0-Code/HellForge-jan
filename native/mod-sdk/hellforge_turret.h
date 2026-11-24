/* HellForge mod SDK 37, 39 and 41: turrets on movers and objects (docs/modding/turrets.md). Included by hellforge_move.h.
 * add_turret sets up a turret entity once: how fast it turns, how far it may turn, how far it sees, what and how fast
 * it fires, and who aims it. It may also put the turret on another entity's mount slot, such as a mover's back.
 * HellForge writes the game's own records (Turret, Rotation, Targeting, SensorEye, the weapon's), so the game's turret
 * AI aims and fires it as it does a sentry. */
#ifndef HELLFORGE_TURRET_H
#define HELLFORGE_TURRET_H

#include <stdint.h>

/* HfTurretDef.aimer: who aims it */
#define HF_AIM_AI 0u     /* the game's turret AI picks targets, aims and fires (a sentry's) */
#define HF_AIM_PLAYER 1u /* only the player in `seat`; its own sight is shut */
#define HF_AIM_BOTH 2u   /* the player in `seat`, and the AI while that seat is empty */
#define HF_AIM_CUSTOM 3u /* only the mod's code; its own sight is shut */

/* HfTurretDef.face (SDK 41): which way it rests once it lands */
#define HF_FACE_GAME 0u /* as the game lands it: a thrown sentry rests facing the world's +Y, whoever threw it */
#define HF_FACE_AWAY 1u /* away from the Helldiver who called it in (the nearest one when it spawns); not with a host */

/* One turret entity (add_turret). Zero-initialize it and set size and entity; every other zero field keeps the game's
 * value. Angles are degrees from the turret's rest, as its TurretComponentData has them (yaw_min is its left limit). */
typedef struct HfTurretDef
{
    uint32_t size;          /* sizeof(HfTurretDef) */
    uint32_t aimer;         /* HF_AIM_AI, HF_AIM_PLAYER, HF_AIM_BOTH or HF_AIM_CUSTOM */
    uint64_t entity;        /* the entity that aims: a sentry, a walker, a turret a vehicle carries */
    uint64_t gun;           /* the entity whose weapon fires, when it is not `entity` (a walker's mounted gun), or 0 */
    uint64_t host;          /* an entity to carry it (a mover, a vehicle, an object), or 0 */
    uint32_t mount_slot;    /* host: the MountComponentData slot it goes in, 0 .. 4 */
    uint32_t attach_node;   /* host: the node it attaches to (a joint's name hash), or 0 for the slot's own */
    uint32_t seat;          /* HF_AIM_PLAYER and HF_AIM_BOTH: the host's seat that aims it, counted from 1 */
    float yaw_speed;        /* its fastest sideways turn, degrees a second */
    float pitch_speed;      /* its fastest turn up and down, degrees a second */
    float pitch_coupling;   /* how much pitching slows while it turns sideways (an exponent) */
    float yaw_min, yaw_max;     /* traverse, -180 .. 180; both 0 keep the game's */
    float pitch_min, pitch_max; /* elevation, -90 .. 90; both 0 keep the game's */
    float body_turn_speed;  /* a walker's body turn onto its target (RotationComponentData), degrees a second */
    float body_turn_accel;  /* how fast that turn speeds up, degrees a second per second */
    float body_turn_decel;  /* how fast it slows down again */
    float range;            /* how far it sees, metres: ahead, to its sides and behind */
    float side_range;       /* a shorter sight to its sides, metres, or 0 for `range` */
    float rear_range;       /* a shorter sight behind it, metres, or 0 for `range` */
    float aim_spring;       /* how hard its aim pulls onto a target (TargetingComponentData; vanilla sentries 10) */
    float aim_damping;      /* how much its aim's swing is held back (vanilla 1; under-damped aim never settles to fire) */
    uint32_t projectile;    /* what its gun fires: a vanilla HF_PROJECTILE_ or add_projectile's type, or 0 for its own */
    float fire_rate;        /* rounds (or beam pulses) a minute */
    uint32_t rounds;        /* rounds a magazine, at most 2047 */
    uint32_t face;          /* SDK 41: HF_FACE_GAME or HF_FACE_AWAY; an SDK 39 HfTurretDef ends before it */
} HfTurretDef;

#endif
