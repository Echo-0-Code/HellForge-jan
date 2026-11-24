/* HellForge mod SDK 35: movers, units HellForge moves itself (docs/modding/movers.md). Included by hellforge_mod.h.
 * add_mover gives every unit of an entity a way to move: walk, drive or hover, a top speed, a turning style, the steps
 * it takes. HellForge moves it from two inputs, throttle and steer: a mod gives them with mover_input, ai_drive hands
 * them to the custom AI (hellforge_ai.h), and a hover vehicle's seated driver gives them too. Walkers and drivers are
 * moved by their root (the game's physics no longer move them); a hover vehicle stays a physics body HellForge pushes. */
#ifndef HELLFORGE_MOVE_H
#define HELLFORGE_MOVE_H

#include <stddef.h>
#include <stdint.h>
#include "hellforge_turret.h" /* SDK 37: turrets, which movers carry */

/* HfMoverDef.mode */
#define HF_MOVE_WALK 1u  /* feet: may step to either side of a block to get round it */
#define HF_MOVE_DRIVE 2u /* wheels or tracks: goes only where it faces; a block stops it */
#define HF_MOVE_HOVER 3u /* SDK 36: a vehicle with a physics body, held hover_height over ground or water by pushes */

/* HfMoverDef.turn */
#define HF_TURN_SPOT 1u    /* turns at turn_rate standing still too (legs, tracks) */
#define HF_TURN_VEHICLE 2u /* turns only while it moves, no tighter than turn_radius (wheels) */

/* HfMoverDef.water (SDK 36): what water deeper than wade_depth does to a walker or driver. A hover always crosses. */
#define HF_WATER_WADE 1u   /* it stops at water deeper than wade_depth (default) */
#define HF_WATER_BLOCK 2u  /* it stays out of water */
#define HF_WATER_CROSS 3u  /* it floats across deep water, at most wade_depth under the surface */
#define HF_WATER_IGNORE 4u /* water is not looked at: it goes along the bottom (SDK 35's way) */

/* HfMoverDef.drivetrain (SDK 36): which axles push a driver. FWD, RWD and 4WD are also HfMoverState.driven's bits. */
#define HF_DRIVETRAIN_FWD 1u  /* the front axle: its grip falls going uphill, as the load moves back */
#define HF_DRIVETRAIN_RWD 2u  /* the rear axle: its grip grows going uphill */
#define HF_DRIVETRAIN_4WD 3u  /* both: all of its weight gives grip */
#define HF_DRIVETRAIN_NONE 4u /* HellForge pushes nothing: HfMoverDef.drive gives the push, or it only coasts */

/* HF_DRIVETRAIN_NONE's push, for one update (a mod's own drive code). */
typedef struct HfMoverPush
{
    uint32_t size;   /* sizeof(HfMoverPush) */
    float forward;   /* m/s2 along its heading (negative backward) */
    float side;      /* m/s2 to its left: a hover only */
    float turn;      /* the turn it wants, radians a second (positive left) */
} HfMoverPush;

struct HfMoverState;
/* Called on the host's game thread each update, before HellForge moves the unit. Don't call mover_* from it. */
typedef void (*HfMoverDriveFn)(HfMod* mod, uint32_t entity, const struct HfMoverState* state, float dt, HfMoverPush* push, void* user);

/* One entity's way of moving (add_mover). Zero-initialize it and set size and entity; zero fields take the defaults,
 * which hang on the mode (docs/modding/movers.md). A mod built against SDK 35 passes the shorter SDK 35 size. */
typedef struct HfMoverDef
{
    uint32_t size;        /* sizeof(HfMoverDef) */
    uint32_t mode;        /* HF_MOVE_WALK (default), HF_MOVE_DRIVE or HF_MOVE_HOVER */
    uint64_t entity;      /* the entity resource its units are */
    float speed;          /* top speed forward, m/s (default 4, at most 40) */
    float reverse_speed;  /* top speed backward, m/s (default half of speed; 0 is the default, so give 0.01 for none) */
    float acceleration;   /* m/s per second, speeding up and slowing down (default 8) */
    uint32_t turn;        /* HF_TURN_SPOT (default) or HF_TURN_VEHICLE */
    float turn_rate;      /* its fastest turn, degrees a second (default 120) */
    float turn_radius;    /* HF_TURN_VEHICLE: its tightest circle's radius, metres (default 4) */
    float step_up;        /* the highest step it climbs, metres (default 0.5) */
    float step_down;      /* the deepest drop it steps down, metres (default 1) */
    float reach;          /* how far ahead of its root its body reaches, metres: walls closer than this block it (default 0.6) */
    float start_delay;    /* seconds after its spawn before HellForge moves it (a hellpod's landing; default 0) */
    /* SDK 36 */
    float brake;          /* m/s2 when it slows down or turns about (default: acceleration) */
    uint32_t water;       /* HF_WATER_WADE (default), _BLOCK, _CROSS or _IGNORE; a hover always crosses */
    float wade_depth;     /* the deepest water it wades, or its draft afloat, metres (default walk 0.8, drive 0.5) */
    uint32_t drivetrain;  /* HF_MOVE_DRIVE: HF_DRIVETRAIN_...; 0 is SDK 35's way, grip never limits it. Hover: 0 or NONE */
    float grip;           /* FWD, RWD, 4WD: tyre friction on the ground (default 0.7, dirt and gravel) */
    HfMoverDriveFn drive; /* HF_DRIVETRAIN_NONE: the mod's own push (NULL: none, it coasts) */
    void* drive_user;     /* passed to drive */
    float hover_height;   /* HF_MOVE_HOVER: its root over the ground or water, metres (default 1.5) */
    float carried;        /* hover: the share of its weight its physics data carries already (VRA extraGravity), 0 .. 1 */
    float mass;           /* hover: its physics body's mass, kg (default 3000, the M-102 FRV's) */
    float hover_hz;       /* hover: how stiffly it holds its height, Hz (default 1.2) */
    float hover_damping;  /* hover: share of critical damping on its height and tilt (default 0.7) */
    float side_drag;      /* hover: how fast a sideways slide dies, 1/s (default 0.5: it drifts) */
} HfMoverDef;

#define HF_MOVER_DEF_SIZE_35 ((uint32_t)offsetof(HfMoverDef, brake))

/* mover_input: what a driver asks of it now. It holds for half a second; call it every update while driving. */
typedef struct HfMoverInput
{
    uint32_t size;        /* sizeof(HfMoverInput) */
    float throttle;       /* -1 full reverse .. 1 full ahead */
    float steer;          /* -1 full right .. 1 full left */
} HfMoverInput;

/* HfMoverState.status (mover_state's answer) */
#define HF_MOVER_NONE 0u     /* not a mover's unit, or gone */
#define HF_MOVER_WAITING 1u  /* its start_delay, or its root not read yet */
#define HF_MOVER_MOVING 2u   /* HellForge moves it (standing still when nothing drives it) */
#define HF_MOVER_BLOCKED 3u  /* the way it drives is a step it can't take, or a wall */
#define HF_MOVER_REMOTE 4u   /* another machine moves it (it is the host's) */

/* HfMoverState.driver */
#define HF_DRIVER_NONE 0u
#define HF_DRIVER_MOD 1u     /* mover_input in the last half second */
#define HF_DRIVER_AI 2u      /* ai_drive */
#define HF_DRIVER_SEAT 3u    /* SDK 36, hover: the player in its driver seat (throttle or steer in the last 2 s) */

typedef struct HfMoverState
{
    uint32_t size;        /* sizeof(HfMoverState) */
    uint32_t status;      /* HF_MOVER_... */
    uint32_t driver;      /* HF_DRIVER_... */
    float position[3];    /* its root in the world */
    float yaw;            /* its heading, radians, counter-clockwise from the world's +Y */
    float speed;          /* m/s along its heading (negative backward) */
    float turning;        /* radians a second (positive left) */
    float throttle, steer; /* the inputs it moves by now */
    /* SDK 36 (an SDK 35 size gets the fields above) */
    float height;         /* hover: its root over the ground or water, metres; walkers and drivers 0 */
    float water_depth;    /* water over the ground where it is, metres (0: dry) */
    float grade;          /* the slope it climbs, rise per metre along its heading (negative downhill) */
    uint32_t driven;      /* HF_DRIVETRAIN_FWD / _RWD bits of the axles pushing now, for wheel animation */
} HfMoverState;

#define HF_MOVER_STATE_SIZE_35 ((uint32_t)offsetof(HfMoverState, height))

/* ai_drive: the custom AI drives a mover's units (add_mover first). The orders are hellforge_ai.h's: Follow me and Hold
 * position from the comms wheel or ai_order, with routes round what is in the way. Zero fields take the defaults. */
typedef struct HfAiDrive
{
    uint32_t size;            /* sizeof(HfAiDrive) */
    uint32_t start_order;     /* HF_ORDER_NONE, or HF_ORDER_FOLLOW (the nearest Helldiver when it spawns) */
    float range;              /* it obeys a speaker within this many metres (default 60) */
    float follow_distance;    /* Follow me keeps it this far from the speaker (default 6) */
    float hold_radius;        /* it may stray this far from its hold point (default 3) */
    uint32_t nav_preset;      /* the size its routes are found for: HF_NAV_TROOPER (default), HF_NAV_VEHICLE, HF_NAV_VEHICLE_WIDE */
    float arrive_distance;    /* it slows down within this many metres of where it goes (default 3) */
} HfAiDrive;

#endif
