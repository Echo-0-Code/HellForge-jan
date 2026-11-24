/* HellForge mod SDK 31-32: orders for AI units and navmesh routes (docs/modding/ai.md). Included by hellforge_mod.h.
 * A unit keeps its own brain: it aims, fights and walks with the game's AI. HellForge only tells it where to go, through
 * the game's own path agent (the navmesh), the way the S.E.A.F. troopers' brains do. Orders come from the comms wheel
 * (Follow me, Hold position) or from the mod (ai_order). Walkers and troopers walk with their navmesh path agent. A unit
 * with none (a sentry the mod walks) gets a navmesh route instead: the mod moves it to ai_state's point. Put a unit on
 * the players' side with its Faction record (HfFaction). */
#ifndef HELLFORGE_AI_H
#define HELLFORGE_AI_H

#include <stdint.h>

/* An order (HfOrderEvent.order, ai_order). */
#define HF_ORDER_NONE 0u   /* no order: the unit's own brain decides where it goes */
#define HF_ORDER_FOLLOW 1u /* keep near an entity (the comms wheel's Follow me) */
#define HF_ORDER_HOLD 2u   /* stay at a point (Hold position): walk back to it when pushed off */
#define HF_ORDER_GOTO 3u   /* walk to a point, then hold there */

/* HfAiState.status */
#define HF_AI_IDLE 0u        /* no order */
#define HF_AI_MOVING 1u      /* walking toward the order's point */
#define HF_AI_THERE 2u       /* at the point, or near enough to the entity it follows */
#define HF_AI_DETOUR 3u      /* the point could not be reached: walking to a nearer one first */
#define HF_AI_UNREACHABLE 4u /* no nearer point worked either: it stands still and tries again later */
#define HF_AI_NO_AGENT 5u    /* no brain on this machine, or neither a path agent nor a route: it cannot be walked */

/* HF_ON_ORDER (SDK 31): a player gave an order on the comms wheel, on every machine of the lobby. HellForge raises it
 * once an order, however it arrives (this machine's wheel, the game's own message, HellForge's). */
typedef struct HfOrderEvent
{
    HfEventInfo info;
    uint32_t order;       /* HF_ORDER_FOLLOW or HF_ORDER_HOLD */
    uint32_t speaker;     /* the speaking player's Helldiver, 0 when it does not resolve here */
    uint64_t peer;        /* the speaking player's peer, 0 when unknown */
    float position[3];    /* the Helldiver's position (zero when speaker is 0) */
    uint32_t local;       /* 1: this machine's player */
} HfOrderEvent;

/* Units of one entity that obey the comms wheel with no code of the mod's (ai_obey). Zero-initialize it and set size;
 * zero fields take the defaults. */
typedef struct HfAiObey
{
    uint32_t size;          /* sizeof(HfAiObey) */
    uint64_t entity;        /* the entity resource its units are (add_entity's, register_entity's or vanilla) */
    float range;            /* a unit obeys a speaker within this many metres (default 60, at most 500) */
    float follow_distance;  /* Follow me keeps it this far from the speaker (default 6, 1 .. 50) */
    float hold_radius;      /* it may stray this far from its hold point (default 3, 0.5 .. 50) */
    /* 0, or the entity resource of a rider the unit carries (a walker's seated pilot): the unit's nearest rider of that
     * resource within 8 m gets the same moves. Some walkers are steered by their pilot's agent too. */
    uint64_t rider;
    uint32_t start_order;   /* the order a new unit starts with: HF_ORDER_NONE, or HF_ORDER_FOLLOW (its caller's Helldiver) */
    /* A unit with no path agent: the size its routes are found for, HF_NAV_TROOPER (the default), HF_NAV_VEHICLE or
     * HF_NAV_VEHICLE_WIDE. The mod walks it to ai_state's point; HellForge only routes it. */
    uint32_t nav_preset;
} HfAiObey;

/* What ai_state answers. */
typedef struct HfAiState
{
    uint32_t size;        /* sizeof(HfAiState) */
    uint32_t order;       /* HF_ORDER_... */
    uint32_t status;      /* HF_AI_... */
    uint32_t target;      /* the entity it follows (HF_ORDER_FOLLOW), else 0 */
    float point[3];       /* where it is sent now (a detour's point while HF_AI_DETOUR) */
    float position[3];    /* where it is */
    float distance;       /* metres from its position to the order's own point (the followed entity's, for FOLLOW) */
    uint32_t moving;      /* 1 while it walks: it moved since the last step (a quarter second) */
    uint32_t behaviour;   /* its brain (behaviour id) */
    /* 1: it has no path agent, so the mod walks it: to point (its route's next corner) while status is MOVING or
     * DETOUR, and it stands while THERE, UNREACHABLE or IDLE. */
    uint32_t steered;
    float goal[3];        /* where its route ends: the order's point less the follow distance, or a detour's point */
} HfAiState;

/* ---- SDK 32: navmesh routes (nav_route_open .. nav_route_close) ---- */

/* HfNavProfile.preset: the sizes the game's own walkers path as. */
#define HF_NAV_TROOPER 1u      /* a trooper: radius 0.79 m */
#define HF_NAV_VEHICLE 2u      /* a vehicle-sized walker: radius 1.05 m */
#define HF_NAV_VEHICLE_WIDE 3u /* a wide vehicle: radius 2.1 m */

/* Who a route is for. Zero-initialize it, set size and one of: preset, entity (its Navigation record), or profile + radius. */
typedef struct HfNavProfile
{
    uint32_t size;    /* sizeof(HfNavProfile) */
    uint32_t preset;  /* HF_NAV_..., or 0 */
    uint64_t entity;  /* an entity resource with a Navigation record (a walker, a trooper), or 0 */
    uint32_t profile; /* a navigation profile hash, with radius */
    float radius;     /* metres, up to 2.6 */
    float value;      /* the profile's companion value (default 0.5) */
} HfNavProfile;

/* nav_route_read's answer (HfNavRoute.status) */
#define HF_ROUTE_NONE 0u      /* nothing asked in this mission yet */
#define HF_ROUTE_SEARCHING 1u /* asked: no answer yet (one or two updates) */
#define HF_ROUTE_READY 2u     /* a route to the goal */
#define HF_ROUTE_PARTIAL 3u   /* a route that stops short of the goal (the goal is off the navmesh or cut off) */
#define HF_ROUTE_FAILED 4u    /* no answer within 3 s */
#define HF_ROUTE_OFF 5u       /* not in a mission, or the game's code is not the reviewed one */

typedef struct HfNavRoute
{
    uint32_t size;   /* sizeof(HfNavRoute) */
    uint32_t status; /* HF_ROUTE_... */
    uint32_t count;  /* its corners, from the walker to the end (at most 64) */
    float length;    /* metres along the corners */
    float end[3];    /* its last corner */
    float goal[3];   /* the goal asked for, on the ground */
} HfNavRoute;

#endif
