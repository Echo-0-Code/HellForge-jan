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
/* SDK 36: drivers whose tyres' grip caps how hard they speed up and brake, and which axles push (HF_MOVE_DRIVE: neither) */
#define HF_MOVE_DRIVE_FWD 4u /* front-wheel drive: its grip falls going uphill, as the load moves back */
#define HF_MOVE_DRIVE_RWD 5u /* rear-wheel drive: its grip grows going uphill */
#define HF_MOVE_DRIVE_4WD 6u /* four-wheel drive: all of its weight gives grip */

/* HfMoverDef.turn */
#define HF_TURN_SPOT 1u    /* turns at turn_rate standing still too (legs, tracks) */
#define HF_TURN_VEHICLE 2u /* turns only while it moves, no tighter than turn_radius (wheels) */

/* HfMoverDef.water (SDK 36): what water deeper than wade_depth does to a walker or driver. A hover always crosses. */
#define HF_WATER_WADE 1u   /* it stops at water deeper than wade_depth (default) */
#define HF_WATER_BLOCK 2u  /* it stays out of water */
#define HF_WATER_CROSS 3u  /* it floats across deep water, at most wade_depth under the surface */
#define HF_WATER_IGNORE 4u /* water is not looked at: it goes along the bottom (SDK 35's way) */

/* HfMoverState.driven (SDK 36): the axles pushing now, for wheel animation */
#define HF_AXLE_FRONT 1u
#define HF_AXLE_REAR 2u

/* HfMoverDef.drive's push, for one update: a mod's own movement in any mode, in place of HellForge's from the inputs. */
typedef struct HfMoverPush
{
    uint32_t size;   /* sizeof(HfMoverPush) */
    float forward;   /* m/s2 along its heading (negative backward) */
    float side;      /* m/s2 to its left: a walker or hover */
    float turn;      /* the turn it wants, radians a second (positive left) */
} HfMoverPush;

struct HfMoverState;
/* Called on the host's game thread each update, before HellForge moves the unit. Don't call mover_* from it. */
typedef void (*HfMoverDriveFn)(HfMod* mod, uint32_t entity, const struct HfMoverState* state, float dt, HfMoverPush* push, void* user);

/* SDK 37: sounds a mover posts on its unit as its state changes: Wwise event ids (hf_wwise_id), 0 none. A loop the
 * moving or idle event starts is the mod's bank's to end, on the next start or stop event. */
typedef struct HfMoverSounds
{
    uint32_t idle;        /* it comes to rest (with stop), and at its start: an idle loop */
    uint32_t moving;      /* it sets off (with start): a moving loop */
    uint32_t start;       /* it sets off */
    uint32_t stop;        /* it comes to rest */
    uint32_t boost;       /* a boost begins */
    uint32_t blocked;     /* a step, wall, slope or water stops it */
} HfMoverSounds;

/* One entity's way of moving (add_mover). Zero-initialize it and set size and entity; zero fields take the defaults,
 * which hang on the mode (docs/modding/movers.md). A mod built against SDK 35 passes the shorter SDK 35 size. */
typedef struct HfMoverDef
{
    uint32_t size;        /* sizeof(HfMoverDef) */
    uint32_t mode;        /* HF_MOVE_WALK (default), HF_MOVE_DRIVE, _DRIVE_FWD, _RWD, _4WD or HF_MOVE_HOVER */
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
    float grip;           /* HF_MOVE_DRIVE_FWD, _RWD, _4WD: tyre friction on the ground (default 0.7, dirt and gravel) */
    HfMoverDriveFn drive; /* any mode: the mod's own push each update instead of HellForge's (NULL: HellForge's) */
    void* drive_user;     /* passed to drive */
    float hover_height;   /* HF_MOVE_HOVER: its root over the ground or water, metres (default 1.5) */
    float carried;        /* hover: the share of its weight its physics data carries already (VRA extraGravity), 0 .. 1 */
    float mass;           /* hover: its physics body's mass, kg (default 3000, the M-102 FRV's) */
    float hover_hz;       /* hover: how stiffly it holds its height, Hz (default 1.2) */
    float hover_damping;  /* hover: share of critical damping on its height and tilt (default 0.7) */
    float side_drag;      /* hover: how fast a sideways slide dies, 1/s (default 0.5: it drifts) */
    /* SDK 37 */
    float max_slope;      /* the steepest incline it climbs, degrees (default: rising step_up in 1 m, 27 for 0.5 m steps) */
    float tilt_limit;     /* the most it tilts, degrees: align_to_ground follows slopes to this, a hover is righted past it (default 30) */
    uint32_t align_to_ground; /* 1: it pitches and rolls with the ground under it; 0 (default): it stays upright, a hover level */
    float fly_height_min; /* hover: the lowest height lift takes it to, metres (default hover_height) */
    float fly_height_max; /* hover: the highest, at most 25 (default hover_height: lift does nothing) */
    float climb_rate;     /* hover: metres a second at full lift (default 2) */
    float boost_speed;    /* top speed while boosting, m/s (default 0: no boost) */
    float boost_time;     /* seconds a full boost lasts; it refills in twice that (default 3) */
    float strafe_speed;   /* walk or hover: top speed sideways at full strafe, m/s (default reverse_speed, hover 5) */
    HfMoverSounds sounds;
} HfMoverDef;

#define HF_MOVER_DEF_SIZE_35 ((uint32_t)offsetof(HfMoverDef, brake))
#define HF_MOVER_DEF_SIZE_36 ((uint32_t)offsetof(HfMoverDef, max_slope))

/* mover_input: what a driver asks of it now. It holds for half a second; call it every update while driving. */
typedef struct HfMoverInput
{
    uint32_t size;        /* sizeof(HfMoverInput) */
    float throttle;       /* -1 full reverse .. 1 full ahead */
    float steer;          /* -1 full right .. 1 full left */
    /* SDK 37 (an SDK 35 size drives with the two above) */
    float strafe;         /* -1 full right .. 1 full left: a walker or hover slides sideways at strafe_speed */
    float lift;           /* -1 down .. 1 up: a hover's height moves at climb_rate between fly_height_min and _max */
    float boost;          /* 1 boosts, while its boost lasts (boost_speed); 0 not */
} HfMoverInput;

#define HF_MOVER_INPUT_SIZE_35 ((uint32_t)offsetof(HfMoverInput, strafe))

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
    uint32_t driven;      /* HF_AXLE_FRONT / _REAR bits of the axles pushing now, for wheel animation */
    /* SDK 37 */
    float pitch, roll;    /* radians, nose up and right side down (align_to_ground, or a hover's body) */
    float boost_left;     /* seconds of boost left */
    uint32_t boosting;    /* 1 while it boosts */
    float strafe, lift;   /* the inputs it moves by now */
} HfMoverState;

#define HF_MOVER_STATE_SIZE_36 ((uint32_t)offsetof(HfMoverState, pitch))

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
    /* SDK 37, fighting. An SDK 35 HfAiDrive ends before these and keeps SDK 35's behaviour: it never moves to fight. */
    float engage_range;       /* it moves onto enemies within this many metres (0: it fires only from where it goes) */
    float keep_distance;      /* engaging: it stands this far from its target (default 12) */
    float leash;              /* fighting: it strays at most this far from whom it follows or its hold point (default 25) */
    float react_time;         /* seconds an enemy stays in range before it moves to engage (default 0.5) */
    float retreat_health;     /* below this share of its health, 0 .. 1, it retreats from enemies near it (0: never) */
    uint32_t retreat_style;   /* HF_RETREAT_RUN (default) or HF_RETREAT_FIGHT */
    uint32_t owner;           /* whose wheel orders it takes: HF_OWNER_ANY (default), HF_OWNER_CALLER or HF_OWNER_CODE */
    float spacing;            /* it keeps this far from other friendly units, Helldivers aside (0: none) */
} HfAiDrive;

/* HfAiDrive.retreat_style */
#define HF_RETREAT_RUN 0u   /* turns away and drives back to whom it follows (or its hold point) */
#define HF_RETREAT_FIGHT 1u /* backs off the same way facing the enemy, so its turret keeps firing */

/* HfAiDrive.owner: `range` is how near a speaker must be */
#define HF_OWNER_ANY 0u     /* any Helldiver's Follow me and Hold position */
#define HF_OWNER_CALLER 1u  /* only its caller's: the Helldiver nearest it when it lands */
#define HF_OWNER_CODE 2u    /* no wheel orders: only ai_order from the mod */

#endif
