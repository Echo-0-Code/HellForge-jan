/* HellForge mod SDK 35: movers, units HellForge moves itself (docs/modding/movers.md). Included by hellforge_mod.h.
 * add_mover gives every unit of an entity a way to move: walk, drive, hover or fly, a top speed, a turning style, the steps
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
#define HF_MOVE_HOVER 3u /* SDK 36: a vehicle with a physics body, held hover.height over ground or water by pushes */
/* SDK 36: drivers whose tyres' grip caps how hard they speed up and brake, and which axles push (HF_MOVE_DRIVE: neither) */
#define HF_MOVE_DRIVE_FWD 4u /* front-wheel drive: its grip falls going uphill, as the load moves back */
#define HF_MOVE_DRIVE_RWD 5u /* rear-wheel drive: its grip grows going uphill */
#define HF_MOVE_DRIVE_4WD 6u /* four-wheel drive: all of its weight gives grip */
/* SDK 43: fliers, physics bodies like a hover, held hover.height over the ground; they take off from the ground */
#define HF_MOVE_HELI 7u  /* rotor or VTOL: hovers in place, tilts toward the way it speeds up, turns on the spot */
#define HF_MOVE_PLANE 8u /* fixed wing: needs stall_speed to climb, banks to turn, rolls along the ground below it */
/* SDK 43: vessels, physics bodies afloat wade_depth (their draft) under the water's surface; aground they sit still */
#define HF_MOVE_BOAT 9u  /* on the surface, steered by its rudder: it turns only while moving */
#define HF_MOVE_SUB 10u  /* a boat that dives: lift takes it down to dive_depth under the surface and back */


/* HfMoverDef.water (SDK 36): what a walker or driver does in water. A hover always crosses over it. */
#define HF_WATER_WADE 1u  /* it wades water up to wade_depth and stops at deeper water (default) */
#define HF_WATER_DRY 2u   /* it stays out of water */
#define HF_WATER_FLOAT 3u /* it floats across deep water, wade_depth (its draft) under the surface */
#define HF_WATER_SINK 4u  /* it goes along the bottom, however deep */

/* HfMoverDef.hover (SDK 39): a hover vehicle's or flier's lift and body. Zero fields take the defaults, which hang on
 * the mode; a flier's heights go up to 500 m and its height_min is its height sitting on the ground. */
typedef struct HfMoverHover
{
    float height;         /* its root over the ground or water, metres (default 1.5) */
    float height_min;     /* the lowest the lift input takes it, metres (default height) */
    float height_max;     /* the highest, at most 25, a flier 500 (default height: lift does nothing; a flier 4 x height) */
    float climb_rate;     /* metres a second at full lift (default 2) */
    float carried;        /* the share of its weight its physics data carries already (VRA extraGravity), 0 .. 1 */
    float mass;           /* its physics body's mass, kg (default 3000, the M-102 FRV's) */
    float stiffness;      /* how stiffly it holds its height and level, Hz (default 1.2) */
    float damping;        /* share of critical damping on its height and tilt (default 0.7) */
    float side_drag;      /* how fast a sideways slide dies, 1/s (default 0.5: it drifts) */
} HfMoverHover;

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
 * which hang on the mode (docs/modding/movers.md). SDK 39 reordered it: build against SDK 39's header. */
typedef struct HfMoverDef
{
    uint32_t size;        /* sizeof(HfMoverDef) */
    uint32_t mode;        /* HF_MOVE_WALK (default), _DRIVE, _DRIVE_FWD, _RWD, _4WD, _HOVER, _HELI, _PLANE, _BOAT or _SUB */
    uint64_t entity;      /* the entity resource its units are */
    float speed;          /* top speed forward, m/s (default 4, at most 40; a flier 120) */
    float reverse_speed;  /* top speed backward, m/s; negative: a share of speed (-0.5 half, -1 as fast); 0: it can't reverse */
    float acceleration;   /* m/s per second speeding up (default 8; hover 3) */
    float turn_rate;      /* its fastest turn, degrees a second (default 120; hover 57) */
    float turn_radius;    /* its tightest circle, metres: it turns only while moving (wheels); 0 turns on the spot */
    float step_up;        /* the highest step it climbs, metres (default 0.5) */
    float step_down;      /* the deepest drop it steps down, metres (default 1) */
    float reach;          /* how far ahead of its root its body reaches, metres: walls closer than this block it (default 0.6) */
    float start_delay;    /* seconds after its spawn before HellForge moves it (a hellpod's landing; default 0) */
    /* SDK 36 */
    float brake;          /* m/s2 when it slows down or turns about (default: acceleration) */
    uint32_t water;       /* HF_WATER_WADE (default), _DRY, _FLOAT or _SINK; a hover or flier always crosses */
    float wade_depth;     /* the deepest water it wades, or its draft afloat, metres (default walk 0.8, drive 0.5) */
    float grip;           /* HF_MOVE_DRIVE_FWD, _RWD, _4WD: tyre friction on the ground (default 0.7, dirt and gravel) */
    HfMoverDriveFn drive; /* any mode: the mod's own push each update instead of HellForge's (NULL: HellForge's) */
    void* drive_user;     /* passed to drive */
    /* SDK 37 */
    float max_slope;      /* the steepest incline it climbs, degrees (default: rising step_up in 1 m, 27 for 0.5 m steps) */
    float tilt_limit;     /* degrees it pitches and rolls with the ground under it, at most; 0 stays upright (a hover level) */
    float boost_speed;    /* top speed while boosting, m/s (default 0: no boost) */
    float boost_time;     /* seconds a full boost lasts; it refills in twice that (default 3) */
    float strafe_speed;   /* walk or hover: top speed sideways at full strafe, m/s (default half of speed, hover 5) */
    HfMoverSounds sounds;
    HfMoverHover hover;   /* HF_MOVE_HOVER, _HELI and _PLANE */
    uint32_t reserved[4]; /* 0; keeps this larger than SDK 37's layout, so that one is refused */
    /* SDK 43 (an SDK 39 size moves with the fields above) */
    float stall_speed;    /* HF_MOVE_PLANE: below this its wings carry it less (lift grows with speed squared), m/s (default 0.45 x speed) */
    float bank_max;       /* HF_MOVE_PLANE: its steepest bank at full steer, degrees; the bank sets its turn (default 40) */
    float dive_depth;     /* HF_MOVE_SUB: the deepest it dives, metres under the surface, at hover.climb_rate (default 3) */
} HfMoverDef;

#define HF_MOVER_DEF_SIZE_39 ((uint32_t)offsetof(HfMoverDef, stall_speed))


/* mover_input: what a driver asks of it now. It holds for half a second; call it every update while driving. */
typedef struct HfMoverInput
{
    uint32_t size;        /* sizeof(HfMoverInput) */
    float throttle;       /* -1 full reverse .. 1 full ahead */
    float steer;          /* -1 full right .. 1 full left */
    /* SDK 37 (an SDK 35 size drives with the two above) */
    float strafe;         /* -1 full right .. 1 full left: a walker or hover slides sideways at strafe_speed */
    float lift;           /* -1 down .. 1 up: a hover's or flier's height moves at hover.climb_rate between hover.height_min and _max */
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
#define HF_DRIVER_SEAT 3u    /* SDK 36, hover or flier: the player in its driver seat (throttle or steer in the last 2 s) */

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
    float height;         /* hover or flier: its root over the ground or water, metres; walkers and drivers 0 */
    float water_depth;    /* water over the ground where it is, metres (0: dry) */
    float grade;          /* the slope it climbs, rise per metre along its heading (negative downhill) */
    uint32_t driven;      /* HF_AXLE_FRONT / _REAR bits of the axles pushing now, for wheel animation */
    /* SDK 37 */
    float pitch, roll;    /* radians, nose up and right side down (tilt_limit, or a hover's body) */
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
