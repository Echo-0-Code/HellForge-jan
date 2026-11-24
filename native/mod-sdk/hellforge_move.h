/* HellForge mod SDK 35: movers, units HellForge moves itself (docs/modding/movers.md). Included by hellforge_mod.h.
 * add_mover gives every unit of an entity a way to move: walk or drive, a top speed, a turning style, the steps it
 * takes. HellForge then moves its root over the ground from two inputs, throttle and steer, on the host. A mod gives
 * them with mover_input, or ai_drive hands them to the custom AI (hellforge_ai.h's orders and routes). The game's own
 * physics no longer move it. */
#ifndef HELLFORGE_MOVE_H
#define HELLFORGE_MOVE_H

#include <stdint.h>

/* HfMoverDef.mode */
#define HF_MOVE_WALK 1u  /* feet: may step to either side of a block to get round it */
#define HF_MOVE_DRIVE 2u /* wheels or tracks: goes only where it faces; a block stops it */

/* HfMoverDef.turn */
#define HF_TURN_SPOT 1u    /* turns at turn_rate standing still too (legs, tracks) */
#define HF_TURN_VEHICLE 2u /* turns only while it moves, no tighter than turn_radius (wheels) */

/* One entity's way of moving (add_mover). Zero-initialize it and set size and entity; zero fields take the defaults. */
typedef struct HfMoverDef
{
    uint32_t size;        /* sizeof(HfMoverDef) */
    uint32_t mode;        /* HF_MOVE_WALK (default) or HF_MOVE_DRIVE */
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
} HfMoverDef;

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
} HfMoverState;

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
