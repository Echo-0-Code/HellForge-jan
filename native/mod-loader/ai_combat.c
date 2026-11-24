/* SDK 37: how ai_drive's units fight (hellforge_move.h's HfAiDrive fighting fields, docs/modding/movers.md). A layer
 * over the orders of ai_orders.c: while it engages or retreats it picks the point the unit drives to, and the orders
 * take over again when it is done. Aiming and firing stay the game's (add_turret). Runs inside the AI update, which
 * holds g_aiLock. Included by world_calls.c after movers.c. */

#define AC_DEFS 32
#define AC_UNITS 64
#define AC_FOES 8

typedef struct AcDef { uint64_t entity; HfAiDrive fight; } AcDef;
typedef struct AcUnit
{
    uint32_t entity, foe, retreating, reversing, placed;
    uint64_t owner;      /* HF_OWNER_CALLER: its caller's player */
    float home[3];       /* with no order: where it stood when it came */
    double inRange;      /* how long an enemy has stayed in engage range */
} AcUnit;

static AcDef g_acDefs[AC_DEFS];
static volatile LONG g_acDefCount;
static AcUnit g_acUnits[AC_UNITS];

static const AcDef* AcDefOf(const AiUnit* u)
{
    const uint64_t resource = u->rule ? g_aiRules[u->rule - 1].def.entity : 0;
    for (LONG i = 0; resource && i < g_acDefCount; i++) if (g_acDefs[i].entity == resource) return &g_acDefs[i];
    return NULL;
}

static AcUnit* AcUnitOf(uint32_t entity, int create)
{
    AcUnit* empty = NULL;
    for (int i = 0; i < AC_UNITS; i++)
    {
        if (g_acUnits[i].entity == entity) return &g_acUnits[i];
        if (!g_acUnits[i].entity && !empty) empty = &g_acUnits[i];
    }
    if (!create || !empty) return NULL;
    memset(empty, 0, sizeof *empty);
    empty->entity = entity;
    return empty;
}

static void AcForget(uint32_t entity)
{
    AcUnit* a = AcUnitOf(entity, 0);
    if (a) memset(a, 0, sizeof *a);
}

/* movers.c: back off facing the enemy instead of turning away. Read without g_aiLock, as AiDrivePoint's point is. */
static int AcReversing(uint32_t entity)
{
    for (int i = 0; entity && i < AC_UNITS; i++) if (g_acUnits[i].entity == entity) return (int)g_acUnits[i].reversing;
    return 0;
}

/* ai_orders.c: whether a wheel order from `peer` reaches it. */
static int AcObeys(const AiUnit* u, uint64_t peer)
{
    const AcDef* d = AcDefOf(u);
    const AcUnit* a = AcUnitOf(u->entity, 0);
    if (d == NULL || d->fight.owner == HF_OWNER_ANY) return 1;
    return d->fight.owner == HF_OWNER_CALLER && a != NULL && a->owner == peer;
}

/* Where its orders keep it: whom it follows, its hold point, or where it came. */
static void AcAnchor(const AiUnit* u, const AcUnit* a, float out[3])
{
    if (u->order == HF_ORDER_FOLLOW && u->target && AiWhere(u->target, out)) return;
    memcpy(out, u->order == HF_ORDER_NONE ? a->home : u->point, 12);
}

/* Its health share, 1 when it does not read. */
static float AcHealth(uint32_t entity, const float pos[3])
{
    HfFind q = {sizeof q, HF_FIND_FRIENDLY | HF_FIND_NEUTRAL | HF_FIND_HOSTILE, HF_FACTION_HELLDIVERS, 0, {pos[0], pos[1], pos[2]}, 4.0f};
    HfFoundEnemy found[AC_FOES];
    const uint32_t n = ApiFindEntities(&g_aiMod, &q, found, AC_FOES);
    for (uint32_t i = 0; i < n; i++)
        if (found[i].entity == entity && found[i].max_health > 0) return (float)found[i].health / (float)found[i].max_health;
    return 1.0f;
}

/* `want` pulled back to within `leash` of the anchor. */
static void AcLeash(float want[3], const float anchor[3], float leash)
{
    const float d = AiFlat(want, anchor);
    if (d <= leash) return;
    want[0] = anchor[0] + (want[0] - anchor[0]) * leash / d;
    want[1] = anchor[1] + (want[1] - anchor[1]) * leash / d;
}

static int AcIsPlayer(uint32_t entity)
{
    uint64_t peer = 0;
    AiPlayerAvatar(0, &peer, entity);
    return peer != 0;
}

/* `want` moved off the friendly units nearer it than `spacing` (Helldivers aside: the follow distance keeps those). */
static int AcSpace(const AiUnit* u, float spacing, float want[3])
{
    if (spacing <= 0) return 0;
    HfFind q = {sizeof q, HF_FIND_FRIENDLY, HF_FACTION_HELLDIVERS, 0, {want[0], want[1], want[2]}, spacing};
    HfFoundEnemy mates[AC_FOES];
    const uint32_t n = ApiFindEntities(&g_aiMod, &q, mates, AC_FOES);
    int moved = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        float d = AiFlat(want, mates[i].position);
        if (mates[i].entity == u->entity || mates[i].entity == u->rider || AcIsPlayer(mates[i].entity) || d >= spacing) continue;
        if (d < 0.1f) d = 0.1f;
        want[0] += (want[0] - mates[i].position[0]) / d * (spacing - d);
        want[1] += (want[1] - mates[i].position[1]) / d * (spacing - d);
        moved = 1;
    }
    return moved;
}

/* Drive to `want`, or stand when it is there. */
static void AcGo(AiUnit* u, AiCtx* c, const float pos[3], const float want[3])
{
    if (AiFlat(pos, want) <= 1.5f) AiArrived(u, c);
    else AiSteer(u, c, pos, want);
}

/* Retreat from `foe`: back toward the anchor, or 15 m straight away from the foe when it stands at the anchor. */
static void AcRetreat(AiUnit* u, AcUnit* a, const AcDef* d, AiCtx* c, const float pos[3], const HfFoundEnemy* foe, const float anchor[3])
{
    float want[3];
    memcpy(want, anchor, 12);
    if (AiFlat(pos, anchor) < 3.0f)
    {
        const float away = AiFlat(pos, foe->position) > 0.1f ? AiFlat(pos, foe->position) : 0.1f;
        want[0] = pos[0] + (pos[0] - foe->position[0]) / away * 15.0f;
        want[1] = pos[1] + (pos[1] - foe->position[1]) / away * 15.0f;
    }
    if (!a->retreating) AiLog(u, "retreats (%s) from %u", d->fight.retreat_style == HF_RETREAT_FIGHT ? "facing it" : "running", foe->entity);
    a->retreating = 1;
    a->reversing = d->fight.retreat_style == HF_RETREAT_FIGHT;
    AcGo(u, c, pos, want);
}

/* Engage `foe`: stand keep_distance from it, inside the leash. */
static void AcEngage(AiUnit* u, AcUnit* a, const AcDef* d, AiCtx* c, const float pos[3], const HfFoundEnemy* foe, const float anchor[3])
{
    const float dist = AiFlat(pos, foe->position) > 0.1f ? AiFlat(pos, foe->position) : 0.1f;
    float want[3] = {foe->position[0] + (pos[0] - foe->position[0]) / dist * d->fight.keep_distance,
        foe->position[1] + (pos[1] - foe->position[1]) / dist * d->fight.keep_distance, pos[2]};
    AcLeash(want, anchor, d->fight.leash);
    AcSpace(u, d->fight.spacing, want);
    if (a->foe != foe->entity) AiLog(u, "engages %u at %.1f m", foe->entity, (double)dist);
    a->foe = foe->entity;
    AcGo(u, c, pos, want);
}

/* Standing (no order, or there): it steps off friendly units closer than its spacing. 1 when it moves. */
static int AcIdleSpace(AiUnit* u, const AcDef* d, AiCtx* c, const float pos[3])
{
    float spaced[3];
    memcpy(spaced, pos, 12);
    if (u->status != HF_AI_THERE && u->order != HF_ORDER_NONE) return 0;
    if (!AcSpace(u, d->fight.spacing, spaced)) return 0;
    AiSteer(u, c, pos, spaced);
    return 1;
}

/* ai_orders.c, every AI step: 1 when it fights (it chose the point), 0 when its orders move it. */
static int AcStep(AiUnit* u, AiCtx* c, const float pos[3])
{
    const AcDef* d = AcDefOf(u);
    AcUnit* a = d ? AcUnitOf(u->entity, 1) : NULL;
    if (a == NULL) return 0;
    if (!a->placed) { a->placed = 1; memcpy(a->home, pos, 12); }
    if (!a->owner) AiPlayerAvatar(0, &a->owner, AiNearestPlayer(pos)); /* its caller stands nearest when it lands */
    a->reversing = 0;
    const float reach = d->fight.engage_range > 0 ? d->fight.engage_range : 30.0f;
    HfFoundEnemy foes[1];
    const int seen = ApiFindEnemies(&g_aiMod, pos, reach, foes, 1) > 0;
    float anchor[3];
    AcAnchor(u, a, anchor);
    if (seen && d->fight.retreat_health > 0 && (a->retreating ? 0.1f : 0.0f) + d->fight.retreat_health > AcHealth(u->entity, pos))
    { AcRetreat(u, a, d, c, pos, foes, anchor); return 1; }
    if (a->retreating) AiLog(u, "stops retreating");
    a->retreating = 0;
    a->inRange = seen && d->fight.engage_range > 0 ? a->inRange + AI_STEP_S : 0;
    if (a->inRange >= d->fight.react_time && seen && AiFlat(anchor, foes->position) <= d->fight.leash + d->fight.keep_distance)
    { AcEngage(u, a, d, c, pos, foes, anchor); return 1; }
    if (a->foe) { AiLog(u, "back to its orders"); a->foe = 0; u->issued = 0; }
    return AcIdleSpace(u, d, c, pos);
}

/* movers.c's ai_drive: the fighting fields, defaults filled in. An SDK 35 HfAiDrive has none (it never fights). */
static const char* AcSet(uint64_t entity, const HfAiDrive* in)
{
    HfAiDrive f;
    memset(&f, 0, sizeof f);
    memcpy(&f, in, in->size < sizeof f ? in->size : sizeof f);
    if (in->size < sizeof f) return NULL;
    if (f.keep_distance == 0) f.keep_distance = 12.0f;
    if (f.leash == 0) f.leash = 25.0f;
    if (f.react_time == 0) f.react_time = 0.5f;
    if (!AiIn(f.engage_range, 0, 300) || !AiIn(f.keep_distance, 1, 200) || !AiIn(f.leash, 1, 500) || !AiIn(f.react_time, 0, 30) ||
        !AiIn(f.retreat_health, 0, 1) || !AiIn(f.spacing, 0, 50) || f.retreat_style > HF_RETREAT_FIGHT || f.owner > HF_OWNER_CODE)
        return "engage_range 0 .. 300, keep_distance 1 .. 200, leash 1 .. 500, react_time 0 .. 30, retreat_health 0 .. 1, spacing 0 .. 50, "
            "retreat_style RUN or FIGHT, owner ANY, CALLER or CODE";
    for (LONG i = 0; i < g_acDefCount; i++) if (g_acDefs[i].entity == entity) { g_acDefs[i].fight = f; return NULL; }
    if (g_acDefCount >= AC_DEFS) return "32 entities fight already";
    g_acDefs[g_acDefCount].entity = entity;
    g_acDefs[g_acDefCount].fight = f;
    InterlockedIncrement(&g_acDefCount);
    return NULL;
}
