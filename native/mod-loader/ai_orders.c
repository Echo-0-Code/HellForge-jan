/* Orders for AI units (SDK 31, docs/modding/ai.md): the comms wheel's Follow me and Hold position, HF_ON_ORDER, ai_obey,
 * ai_order and ai_state. Included at the end of world_calls.c (WcEntity, EvpUnitPosition, the channel's send line).
 * A unit keeps its brain; HellForge hands its path agent a point through the game's own BehaviourMoveTo, as the S.E.A.F.
 * brains do (Automaton brains never call it). A point goes again only when it moved: a path takes several AI steps, and
 * one asked every half second never finished for a far point. A unit without headway gets nearer points, then rests.
 * Orders come three ways, kept once by speaker and order: this machine's wheel (CommsWheelPick), the game's own
 * rpc_trigger_avatar_comms_notify (sent in some missions only) and HellForge's "@hellforge.ai" line. Each machine walks
 * only the units it simulates. Build 25480438. */

#define AI_UNITS 64
#define AI_RULES 32
#define AI_PENDING 64
#define AI_HEARD 16
#define AI_CHANNEL "@hellforge.ai" /* no mod can have it: mod ids start with a letter or digit */
#define AI_WIRE_VERSION 1u
#define AI_BEHAVIOURS 0x3326740u   /* behaviour records: map +0x40, infos +0x58, records +0x60 stride 0x1F8 */
#define AI_AGENTS 0x3326418u       /* path agents: map +0x30, records +0x50 stride 0xA8 (+0x60 goal, +0x90 a request pending) */
#define AI_COMMS 0x3317C30u        /* the comms items, 0x20 bytes each, +0 the id */
#define AI_COMMS_FOLLOW 0xD6208139u
#define AI_COMMS_HOLD 0x0DAE042Eu
#define AI_EVENT_FOLLOW 0xE5764925u /* what the game sends for them */
#define AI_EVENT_HOLD 0x71382183u
#define AI_RPC_COMMS 0xD48CEDB2u    /* rpc_trigger_avatar_comms_notify */
#define AI_STEP_S 0.25
#define AI_AIRBORNE_M 25.0f          /* a Helldiver this high above it is still in its reinforce pod: it waits for the landing */
#define AI_STUCK_S 4.0              /* no headway for this long: the path failed */
#define AI_RETRY_S 10.0             /* an unreachable point is tried again after this */
#define AI_LOG_S 5.0
#define AI_TRAIL 24                 /* points where a followed entity stood */
#define AI_TRAIL_STEP_M 4.0f        /* a new trail point after it moved this far */
#define AI_CORNERS 16
#define AI_REASK_S 2.0              /* a steered unit's failed route is asked again after this */
#define AI_FAR_M 16.0f              /* this far past the follow distance, a unit walks the trail */

typedef struct AiRule { HfMod* mod; HfAiObey def; } AiRule;
typedef struct AiUnit
{
    HfMod* mod;
    uint32_t entity, rider, rule; /* rule: its ai_obey row + 1, 0 none */
    uint32_t order, target, status, issued, tries, detour, moved, spared, selfKills;
    float point[3], goal[3], sent[3], detourAt[3], lastPos[3], headwayFrom[3];
    float follow, hold;
    float trail[AI_TRAIL][3];
    uint32_t trailCount, onTrail;
    uint64_t peer;               /* FOLLOW: the followed Helldiver's player, to follow again after a respawn */
    uint32_t route, corners;     /* a unit with no path agent: its route (nav_routes.c) and the corners read */
    float corner[AI_CORNERS][3];
    double sinceAsk;
    double sinceSent, sinceHeadway, retryIn, sinceLog;
} AiUnit;
typedef struct AiHeard { uint32_t speaker, order; ULONGLONG at; } AiHeard;
typedef struct AiWire { uint8_t version, order; uint16_t reserved; uint32_t network; } AiWire;
typedef struct AiCtx { uint8_t* info; uint8_t* p; } AiCtx;
typedef void (*AiMoveFn)(AiCtx* ctx, const float* point);
typedef void (*AiStopFn)(AiCtx* ctx);
typedef float* (*AiOwnPosFn)(float out[3], AiCtx* ctx);
typedef uint64_t (*AiWheelFn)(void* state, uint32_t item, uint64_t r8, uint64_t r9);

static AiRule g_aiRules[AI_RULES];
static volatile LONG g_aiRuleCount;
static AiUnit g_aiUnits[AI_UNITS];
static struct { uint32_t entity; uint64_t resource; double at; } g_aiPending[AI_PENDING];
static AiHeard g_aiHeard[AI_HEARD];
static AiWire g_aiInbox[32];
static uint64_t g_aiInboxFrom[32]; /* the pipe's sender of each inbox order */
static uint32_t g_aiInboxCount;
static SRWLOCK g_aiLock = SRWLOCK_INIT;      /* the units, held by the game thread's whole update */
static SRWLOCK g_aiInboxLock = SRWLOCK_INIT;
static HfMod g_aiMod;
static int MvWhere(uint32_t entity, float out[3]); /* movers.c: a mover's unit this machine moves */
static int MvPending(uint32_t entity);
static int MvSettling(uint32_t entity);
static int AcStep(AiUnit* u, AiCtx* c, const float pos[3]); /* ai_combat.c: SDK 37 fighting */
static int AcObeys(const AiUnit* u, uint64_t peer);
static void AcForget(uint32_t entity);
static int AcReversing(uint32_t entity);
static const char* AcSet(uint64_t entity, const HfAiDrive* in);
static volatile LONG g_aiTicking;
static int g_aiState;                        /* 0 waiting for the game, 1 ready, -1 off (logged) */
static int g_aiHooked, g_aiListening;
static int g_aiSpawnsHeard; /* HF_ON_SPAWN subscribed, once ai_obey has a rule */
static double g_aiClock, g_aiStep;
static uintptr_t g_aiGame;
static AiMoveFn g_aiMove;
static AiStopFn g_aiStop;
static AiOwnPosFn g_aiOwnPos;
static AiWheelFn g_aiWheelNext;

static float AiDist(const float* a, const float* b) { float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2]; return sqrtf(x * x + y * y + z * z); }
static float AiFlat(const float* a, const float* b) { float x = a[0] - b[0], y = a[1] - b[1]; return sqrtf(x * x + y * y); }
static int AiIn(float v, float low, float high) { return v >= low && v <= high; }

/* ---- the game's records ---- */

/* The game's u32 maps {table, capacity, empty key, multiplier}: an entity's index, -1 none. */
static int64_t AiMapFind(uintptr_t header, uint32_t key)
{
    uint8_t h[20];
    uint64_t table;
    uint32_t cap, empty, mult;
    if (SafeRead(header, h, sizeof h)) return -1;
    memcpy(&table, h, 8); memcpy(&cap, h + 8, 4); memcpy(&empty, h + 12, 4); memcpy(&mult, h + 16, 4);
    if (table < 0x10000 || cap == 0 || cap > (1u << 20) || (cap & (cap - 1))) return -1;
    for (uint32_t i = 0; i < cap && i < 4096; i++)
    {
        uint32_t kv[2], slot = (i + mult * key) & (cap - 1);
        if (SafeRead((uintptr_t)table + (uintptr_t)slot * 8, kv, 8)) return -1;
        if (kv[0] == key) return kv[1] == 0xFFFFFFFFu ? -1 : (int64_t)kv[1];
        if (kv[0] == empty) return -1;
    }
    return -1;
}

static uintptr_t AiPtr(uintptr_t at) { uintptr_t v = 0; return SafeRead(at, &v, 8) ? 0 : v; }

/* An entity's behaviour ctx; 0 when it has no behaviour record here. */
static int AiCtxOf(uint32_t entity, AiCtx* ctx)
{
    uintptr_t m = AiPtr(g_aiGame + AI_BEHAVIOURS), info, records, infos;
    int64_t i = m ? AiMapFind(m + 0x40, entity) : -1;
    uint32_t named = 0;
    if (i < 0 || !(infos = AiPtr(m + 0x58)) || !(records = AiPtr(m + 0x60)) || !(info = AiPtr(infos + (uintptr_t)i * 8)) ||
        SafeRead(info + 8, &named, 4) || named != entity) return 0;
    ctx->info = (uint8_t*)info;
    ctx->p = (uint8_t*)(records + (uintptr_t)i * 0x1F8 + 8);
    return 1;
}
static int AiAuthority(const AiCtx* c) { uint32_t f = 0; return !SafeRead((uintptr_t)c->info + 0x14, &f, 4) && (f & 1); }
static uint32_t AiBehaviour(const AiCtx* c) { uint32_t id = 0; SafeRead((uintptr_t)c->p - 8, &id, 4); return id; }

/* Its navmesh path agent's record, 0 none. */
static uintptr_t AiAgentOf(uint32_t entity)
{
    uintptr_t m = AiPtr(g_aiGame + AI_AGENTS), records = m ? AiPtr(m + 0x50) : 0;
    int64_t i = m ? AiMapFind(m + 0x30, entity) : -1;
    return i >= 0 && records ? records + (uintptr_t)i * 0xA8 : 0;
}
/* The goal its path agent walks to: MoveTo writes it, Stop writes the unit's own position. 0 when there is no agent. */
static int AiAgentGoal(uint32_t entity, float out[3]) { uintptr_t a = AiAgentOf(entity); return a && !SafeRead(a + 0x60, out, 12); }

static int AiOwnPos(AiCtx* c, float out[3])
{
    AiCtx copy = *c;
    g_aiOwnPos(out, &copy);
    for (int k = 0; k < 3; k++) if (!isfinite(out[k]) || fabsf(out[k]) > 100000.0f) return 0;
    return 1;
}

/* Any entity's position (its unit's root). */
static int AiWhere(uint32_t entity, float out[3])
{
    uint32_t unit = 0, network = 0;
    return entity && EvpAlive(entity) && WcEntity(entity, &unit, &network) && EvpUnitPosition(unit, out);
}

/* The S.E.A.F. brains answer the wheel themselves (their event functions take the AI events). */
static int AiNativeObeys(uint32_t behaviour)
{
    static const uint32_t ids[] = {615, 616, 617, 618, 623, 630, 631};
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) if (ids[i] == behaviour) return 1;
    return 0;
}

/* ---- moving a unit ---- */

static void AiSend(AiUnit* u, AiCtx* c, const float point[3])
{
    if (u->route) { ApiNavRouteAsk(&g_aiMod, u->route, u->lastPos, point); u->corners = 0; u->sinceAsk = 0; }
    else g_aiMove(c, point);
    AiCtx rc;
    if (!u->route && u->rider && AiCtxOf(u->rider, &rc)) g_aiMove(&rc, point);
    memcpy(u->sent, point, 12);
    u->issued = 1;
    u->sinceSent = 0;
}

static void AiHalt(AiUnit* u, AiCtx* c)
{
    if (!u->issued) return;
    u->corners = 0;
    if (u->route) { u->issued = 0; return; } /* the mod stands it while THERE or UNREACHABLE */
    g_aiStop(c);
    AiCtx rc;
    if (u->rider && AiCtxOf(u->rider, &rc)) g_aiStop(&rc);
    u->issued = 0;
}

/* A nearer point after a failed path: halfway, then off to each side of the straight line. */
static void AiDetourPoint(const float from[3], const float to[3], uint32_t attempt, float out[3])
{
    float dx = to[0] - from[0], dy = to[1] - from[1], len = sqrtf(dx * dx + dy * dy);
    float side = attempt == 2 ? 6.0f : attempt == 3 ? -6.0f : 0.0f, along = attempt == 1 ? 0.5f : 0.4f;
    float px = len > 0.1f ? -dy / len : 0.0f, py = len > 0.1f ? dx / len : 0.0f;
    out[0] = from[0] + dx * along + px * side;
    out[1] = from[1] + dy * along + py * side;
    out[2] = from[2] + (to[2] - from[2]) * along;
}

static void AiLog(AiUnit* u, const char* format, ...)
{
    char text[400];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    ApiLog(u->mod ? u->mod : &g_aiMod, "ai: unit %u: %s", u->entity, text);
}

static const char* AiStatusName(uint32_t s)
{
    static const char* names[] = {"idle", "moving", "there", "detour", "unreachable", "no path agent"};
    return s < sizeof names / sizeof names[0] ? names[s] : "?";
}

/* An unreachable unit's wait: 1 while it rests, 0 when it tries again (the wait ran out, or the point moved 5 m). */
static int AiResting(AiUnit* u, const float want[3])
{
    if (u->status != HF_AI_UNREACHABLE) return 0;
    if ((u->retryIn -= AI_STEP_S) > 0 && AiDist(want, u->goal) < 5.0f) return 1;
    u->status = HF_AI_MOVING;
    u->tries = 0;
    u->issued = 0;
    return 0;
}

/* No headway for AI_STUCK_S: the agent found no path to its goal. A nearer point, or rest. */
static void AiNoHeadway(AiUnit* u, AiCtx* c, const float pos[3], const float want[3])
{
    float goal[3] = {0, 0, 0};
    AiAgentGoal(u->entity, goal);
    u->sinceHeadway = 0;
    if (++u->tries <= 3)
    {
        AiDetourPoint(pos, want, u->tries, u->detourAt);
        u->detour = 1;
        u->issued = 0;
        AiLog(u, "no headway for %.0f s toward %.1f %.1f %.1f (path agent's goal %.1f %.1f %.1f): detour %u to %.1f %.1f %.1f",
            AI_STUCK_S, (double)want[0], (double)want[1], (double)want[2], (double)goal[0], (double)goal[1], (double)goal[2], u->tries,
            (double)u->detourAt[0], (double)u->detourAt[1], (double)u->detourAt[2]);
        return;
    }
    u->detour = 0;
    u->tries = 0;
    u->status = HF_AI_UNREACHABLE;
    u->retryIn = AI_RETRY_S;
    AiHalt(u, c);
    AiLog(u, "can't reach %.1f %.1f %.1f: standing at %.1f %.1f %.1f, trying again in %.0f s", (double)want[0], (double)want[1],
        (double)want[2], (double)pos[0], (double)pos[1], (double)pos[2], AI_RETRY_S);
}

/* Toward `want` (the order's point now), around failed paths. */
static void AiSteer(AiUnit* u, AiCtx* c, const float pos[3], const float want[3])
{
    if (AiResting(u, want)) return;
    memcpy(u->goal, want, 12);
    if (u->detour && AiFlat(pos, u->detourAt) < 2.5f) { u->detour = 0; u->issued = 0; } /* there: the real point again */
    const float* aim = u->detour ? u->detourAt : want;
    float away = AiFlat(pos, aim), slack = away * 0.15f > 2.0f ? away * 0.15f : 2.0f;
    if (!u->issued) { memcpy(u->headwayFrom, pos, 12); u->sinceHeadway = 0; } /* a following point moving on is no headway */
    float goal[3];
    int replaced = u->sinceSent > (u->onTrail ? 0.5 : 2.0) && AiAgentGoal(u->entity, goal) && AiDist(goal, u->sent) > 1.0f; /* its brain or a stop */
    if (!u->issued || AiDist(aim, u->sent) > slack || replaced) AiSend(u, c, aim);
    u->status = u->detour ? HF_AI_DETOUR : HF_AI_MOVING;
    const int settling = u->route && MvSettling(u->entity); /* a mover starting, or turning on the spot */
    if (settling || AiFlat(pos, u->headwayFrom) > 1.0f) { memcpy(u->headwayFrom, pos, 12); u->sinceHeadway = 0; return; }
    if ((u->sinceHeadway += AI_STEP_S) >= AI_STUCK_S) AiNoHeadway(u, c, pos, want);
}

static void AiArrived(AiUnit* u, AiCtx* c)
{
    if (u->status != HF_AI_THERE) { u->tries = 0; u->detour = 0; }
    u->status = HF_AI_THERE;
    AiHalt(u, c);
}

/* The followed entity's trail: where it stood, every AI_TRAIL_STEP_M, oldest first. It walked there, so the ground holds
 * a path even where the straight line to it climbs a cliff. Near it, only its newest point is kept. */
static void AiTrailAdd(AiUnit* u, const float at[3], float away)
{
    if (away <= u->follow + AI_FAR_M) { memcpy(u->trail[0], at, 12); u->trailCount = 1; return; }
    if (u->trailCount && AiDist(u->trail[u->trailCount - 1], at) < AI_TRAIL_STEP_M) return;
    if (u->trailCount == AI_TRAIL) { memmove(u->trail[0], u->trail[1], sizeof u->trail[0] * (AI_TRAIL - 1)); u->trailCount--; }
    memcpy(u->trail[u->trailCount++], at, 12);
}

/* Far behind: the trail point to walk to. The nearest one, or the one after it once there; the older ones are dropped. 0
 * when the unit is near enough to go straight. A unit on the trail takes its path back from its own brain sooner, since
 * its combat moves drag it off. */
static int AiTrailNext(AiUnit* u, const float pos[3], float away, float out[3])
{
    int behind = away > u->follow + AI_FAR_M && u->trailCount > 1;
    if (behind != (int)u->onTrail) { u->onTrail = behind; u->tries = 0; u->detour = 0; u->issued = 0; }
    if (!behind) return 0;
    uint32_t closest = 0;
    for (uint32_t i = 1; i < u->trailCount; i++)
        if (AiFlat(pos, u->trail[i]) < AiFlat(pos, u->trail[closest])) closest = i;
    if (AiFlat(pos, u->trail[closest]) < 3.0f && closest + 1 < u->trailCount) closest++;
    if (closest > 0) { memmove(u->trail[0], u->trail[closest], sizeof u->trail[0] * (u->trailCount - closest)); u->trailCount -= closest; }
    memcpy(out, u->trail[0], 12);
    return 1;
}

static void AiFollowStep(AiUnit* u, AiCtx* c, const float pos[3], const float at[3])
{
    memcpy(u->point, at, 12);
    float d = AiFlat(pos, at), crumb[3];
    AiTrailAdd(u, at, d);
    if (d <= u->follow + 2.0f) AiArrived(u, c);
    else if (AiTrailNext(u, pos, d, crumb)) AiSteer(u, c, pos, crumb);
    else
    {
        float want[3] = {at[0] + (pos[0] - at[0]) / d * u->follow, at[1] + (pos[1] - at[1]) / d * u->follow, at[2]};
        AiSteer(u, c, pos, want);
    }
}

static void AiStatusLog(AiUnit* u, const float pos[3])
{
    u->sinceLog = 0;
    static const char* orders[] = {"none", "FOLLOW", "HOLD", "GOTO"};
    float goal[3] = {0, 0, 0};
    AiAgentGoal(u->entity, goal);
    AiLog(u, "%s %s, at %.1f %.1f %.1f, %.1f m from the order's point, sent to %.1f %.1f %.1f (path agent's goal %.1f %.1f %.1f),"
        " moving %d, trail %s (%u points), route corners %u", orders[u->order & 3], AiStatusName(u->status), (double)pos[0], (double)pos[1],
        (double)pos[2], (double)AiFlat(pos, u->point), (double)u->sent[0], (double)u->sent[1], (double)u->sent[2], (double)goal[0],
        (double)goal[1], (double)goal[2], u->moved, u->onTrail ? "on" : "off", u->trailCount, u->corners);
}

/* A steered unit's route: its corners once found, asked again when it failed or the mission's agents went. */
static void AiRouteStep(AiUnit* u, const float pos[3])
{
    if (!u->issued) return;
    HfNavRoute r = {sizeof r};
    uint32_t status = ApiNavRouteRead(&g_aiMod, u->route, pos, &r, &u->corner[0][0], AI_CORNERS);
    if (status == HF_ROUTE_READY || status == HF_ROUTE_PARTIAL) u->corners = r.count < AI_CORNERS ? r.count : AI_CORNERS;
    else if (status == HF_ROUTE_NONE || (status == HF_ROUTE_FAILED && (u->sinceAsk += AI_STEP_S) >= AI_REASK_S))
    { ApiNavRouteAsk(&g_aiMod, u->route, pos, u->sent); u->sinceAsk = 0; }
}

/* Where a steered unit walks now: its route's first corner over a metre away, else straight to the point sent. */
static void AiNextPoint(const AiUnit* u, float out[3])
{
    memcpy(out, u->sent, 12);
    for (uint32_t i = 1; i < u->corners; i++)
        if (AiFlat(u->lastPos, u->corner[i]) > 1.0f) { memcpy(out, u->corner[i], 12); return; }
}

/* Its brain's ctx and position when this machine simulates it; 0 when it has no brain record here. */
static int AiPlace(uint32_t entity, AiCtx* c, float pos[3])
{
    return AiCtxOf(entity, c) && AiAuthority(c) && AiOwnPos(c, pos);
}

static AiUnit* AiUnitOf(uint32_t entity, int create);

/* movers.c: where the AI sends a driven unit now, and its status. 0 when it doesn't obey (or the update holds it). */
static int AiDrivePoint(uint32_t entity, float out[3], uint32_t* status)
{
    if (!TryAcquireSRWLockShared(&g_aiLock)) return 0;
    const AiUnit* u = AiUnitOf(entity, 0);
    int ok = u != NULL && u->route != 0;
    if (ok) { AiNextPoint(u, out); *status = u->status; }
    ReleaseSRWLockShared(&g_aiLock);
    return ok;
}

/* Where a unit stands, when this machine walks it: its brain with a path agent or a route, or (no brain record) its mover. */
static int AiStepPlace(const AiUnit* u, AiCtx* c, float pos[3])
{
    if (AiPlace(u->entity, c, pos)) return u->route || AiAgentOf(u->entity);
    return u->route && MvWhere(u->entity, pos); /* a mover's unit: HellForge moves it */
}

static uint32_t AiNearestPlayer(const float at[3]);
static uint32_t AiPlayerAvatar(uint64_t peer, uint64_t* peerOf, uint32_t avatar);

/* A FOLLOW keeps its order when its Helldiver dies: it follows that player's next Helldiver, else the nearest one standing.
 * Only a Hold position order holds it. Any other entity is followed while it exists. 1 when it has someone to follow. */
static int AiFollowWho(AiUnit* u, const float pos[3], float at[3])
{
    if (u->target && (u->peer ? AiPlayerAvatar(u->peer, NULL, 0) == u->target : 1) && AiWhere(u->target, at))
        return at[2] - pos[2] < AI_AIRBORNE_M; /* not a player: while it exists */
    uint32_t next = u->peer ? AiPlayerAvatar(u->peer, NULL, 0) : 0;
    if (!next) next = AiNearestPlayer(pos);
    if (next != u->target && u->target) AiLog(u, "the Helldiver it followed (%u) is down: %s", u->target, next ? "following the next one" : "waiting here");
    if (next != u->target) { u->target = next; u->trailCount = 0; u->onTrail = 0; u->issued = 0; }
    if (next) AiPlayerAvatar(0, &u->peer, next);
    return next && AiWhere(next, at) && at[2] - pos[2] < AI_AIRBORNE_M;
}

/* One unit, every AI_STEP_S; 0 when it is gone. */
static int AiUnitStep(AiUnit* u)
{
    if (!EvpAlive(u->entity)) return 0;
    AiCtx c;
    float pos[3], at[3];
    if (!AiStepPlace(u, &c, pos)) { u->status = u->order ? HF_AI_NO_AGENT : HF_AI_IDLE; return 1; }
    u->moved = AiDist(pos, u->lastPos) > 0.05f;
    memcpy(u->lastPos, pos, 12);
    u->sinceSent += AI_STEP_S;
    if (AcStep(u, &c, pos)) { /* it fights: ai_combat.c chose its point */ }
    else if (u->order == HF_ORDER_NONE) { u->status = HF_AI_IDLE; return 1; }
    else if (u->order == HF_ORDER_FOLLOW && !AiFollowWho(u, pos, at)) AiArrived(u, &c); /* nobody standing: it waits */
    else if (u->order == HF_ORDER_FOLLOW) AiFollowStep(u, &c, pos, at);
    else if (AiFlat(pos, u->point) <= u->hold) AiArrived(u, &c);
    else AiSteer(u, &c, pos, u->point);
    if (u->route) AiRouteStep(u, pos);
    if ((u->sinceLog += AI_STEP_S) >= AI_LOG_S) AiStatusLog(u, pos);
    return 1;
}

static void AiForget(AiUnit* u)
{
    AcForget(u->entity);
    if (u->route) ApiNavRouteClose(&g_aiMod, u->route);
    memset(u, 0, sizeof *u);
}

static AiUnit* AiUnitOf(uint32_t entity, int create)
{
    AiUnit* empty = NULL;
    for (int i = 0; i < AI_UNITS; i++)
    {
        if (g_aiUnits[i].entity == entity) return &g_aiUnits[i];
        if (!g_aiUnits[i].entity && !empty) empty = &g_aiUnits[i];
    }
    if (!create || !empty) return NULL;
    memset(empty, 0, sizeof *empty);
    empty->entity = entity;
    empty->follow = 6.0f;
    empty->hold = 3.0f;
    return empty;
}

/* An order for one unit (under g_aiLock, game thread). */
static void AiSetOrder(AiUnit* u, uint32_t order, uint32_t target, const float point[3], const char* from)
{
    AiCtx c;
    float pos[3] = {0, 0, 0};
    int here = AiCtxOf(u->entity, &c) && AiOwnPos(&c, pos);
    if (here) AiHalt(u, &c); /* the last order's point is no longer wanted */
    u->order = order;
    u->target = order == HF_ORDER_FOLLOW ? target : 0;
    u->peer = 0;
    if (u->target) AiPlayerAvatar(0, &u->peer, u->target);
    if (order == HF_ORDER_HOLD && point == NULL) memcpy(u->point, pos, 12);
    else if (point != NULL) memcpy(u->point, point, 12);
    u->status = order ? HF_AI_MOVING : HF_AI_IDLE;
    u->tries = 0; u->detour = 0; u->issued = 0; u->sinceHeadway = 0; u->sinceLog = AI_LOG_S;
    u->trailCount = 0; u->onTrail = 0;
    static const char* orders[] = {"none", "FOLLOW", "HOLD", "GOTO"};
    AiLog(u, "order %s from %s (behaviour %u)%s", orders[order & 3], from, here ? AiBehaviour(&c) : 0, here ? "" : ": not simulated here yet");
}

/* ---- orders heard ---- */

/* 1 when the same speaker gave the same order in the last 1.5 s (another route brought it already). */
static int AiHeardBefore(uint32_t speaker, uint32_t order)
{
    ULONGLONG now = GetTickCount64();
    AiHeard* oldest = &g_aiHeard[0];
    for (int i = 0; i < AI_HEARD; i++)
    {
        if (g_aiHeard[i].speaker == speaker && g_aiHeard[i].order == order && now - g_aiHeard[i].at < 1500) return 1;
        if (g_aiHeard[i].at < oldest->at) oldest = &g_aiHeard[i];
    }
    *oldest = (AiHeard){speaker, order, now};
    return 0;
}

static uint32_t AiOrderOf(uint32_t id)
{
    if (id == AI_COMMS_FOLLOW || id == AI_EVENT_FOLLOW) return HF_ORDER_FOLLOW;
    if (id == AI_COMMS_HOLD || id == AI_EVENT_HOLD) return HF_ORDER_HOLD;
    return HF_ORDER_NONE;
}

/* The game thread: an order from a speaker (a Helldiver), raised once and handed to the units that obey. */
static void AiHeardOrder(uint32_t order, uint32_t speaker, uint64_t peer, int local, const char* route)
{
    if (!order || !speaker || AiHeardBefore(speaker, order)) return;
    HfOrderEvent e = {{sizeof e, HF_ON_ORDER, 0}, order, speaker, peer, {0, 0, 0}, (uint32_t)local};
    int placed = AiWhere(speaker, e.position);
    Note("ai: %s from Helldiver %u (%s)", order == HF_ORDER_FOLLOW ? "Follow me" : "Hold position", speaker, route);
    if (AnyListener(HF_ON_ORDER)) Raise(&e.info);
    if (!placed) return;
    AcquireSRWLockExclusive(&g_aiLock);
    for (int i = 0; i < AI_UNITS; i++)
    {
        AiUnit* u = &g_aiUnits[i];
        if (!u->entity || !u->rule) continue;
        const AiRule* r = &g_aiRules[u->rule - 1];
        float pos[3];
        AiCtx c;
        if (!AiCtxOf(u->entity, &c) || !AiAuthority(&c) || AiNativeObeys(AiBehaviour(&c)) || !AiOwnPos(&c, pos) || AiDist(pos, e.position) > r->def.range ||
            !AcObeys(u, peer)) continue;
        AiSetOrder(u, order, speaker, NULL, route);
    }
    ReleaseSRWLockExclusive(&g_aiLock);
}

/* This machine's player, 0 when it has no Helldiver. */
static uint32_t AiLocalAvatar(uint64_t* peer, uint32_t* network)
{
    EvpPlayer players[EVT_MAX_PLAYERS];
    int n = EvpPlayers(g_aiGame, players);
    for (int i = 0; i < n; i++)
        if (players[i].local && players[i].avatar)
        {
            uint32_t unit = 0;
            *peer = players[i].peer;
            if (!WcEntity(players[i].avatar, &unit, network)) *network = WC_NO_NETWORK;
            return players[i].avatar;
        }
    return 0;
}

static uint64_t AiWheelDetour(void* state, uint32_t item, uint64_t r8, uint64_t r9)
{
    uint32_t id = 0;
    if (item < 4096) SafeRead(g_aiGame + AI_COMMS + (uintptr_t)item * 0x20, &id, 4);
    uint32_t order = AiOrderOf(id);
    if (order)
    {
        uint64_t peer = 0;
        uint32_t network = WC_NO_NETWORK, avatar = AiLocalAvatar(&peer, &network);
        if (avatar) AiHeardOrder(order, avatar, peer, 1, "this machine's comms wheel");
        uint32_t role;
        AcquireSRWLockShared(&g_sessionLock);
        role = g_session.role;
        ReleaseSRWLockShared(&g_sessionLock);
        if (avatar && network != WC_NO_NETWORK && role != HF_SESSION_SOLO)
        {
            AiWire wire = {AI_WIRE_VERSION, (uint8_t)order, 0, network};
            char hex[sizeof wire * 2 + 1], line[64 + sizeof hex];
            ChannelHex((const uint8_t*)&wire, sizeof wire, hex);
            snprintf(line, sizeof line, "send\t%s\tall\t%s", AI_CHANNEL, hex);
            LiveQueue(line);
        }
    }
    return g_aiWheelNext(state, item, r8, r9);
}

static int AiRpc(HfMod* mod, const HfRpcEvent* e, void* user)
{
    (void)mod; (void)user;
    int32_t network = 0;
    uint32_t id = 0;
    if (e->count != 2 || e->args[0].value == NULL || e->args[1].value == NULL) return HF_RPC_KEEP;
    memcpy(&network, e->args[0].value, 4);
    memcpy(&id, e->args[1].value, 4);
    uint32_t order = AiOrderOf(id), speaker = order ? ApiEntityOfNetwork(NULL, (uint32_t)network) : 0xFFFFFFFFu;
    if (order && speaker != 0xFFFFFFFFu) AiHeardOrder(order, speaker, e->sender, 0, "the game's comms message");
    return HF_RPC_KEEP;
}

/* The other machines' wheel orders, as the live pipe delivers them (its thread): queued for the game thread. */
static void AiReceive(uint64_t from, const uint8_t* data, uint32_t bytes)
{
    AiWire wire;
    if (bytes != sizeof wire) return;
    memcpy(&wire, data, sizeof wire);
    if (wire.version != AI_WIRE_VERSION || (wire.order != HF_ORDER_FOLLOW && wire.order != HF_ORDER_HOLD)) return;
    AcquireSRWLockExclusive(&g_aiInboxLock);
    if (g_aiInboxCount < 32) { g_aiInboxFrom[g_aiInboxCount] = from; g_aiInbox[g_aiInboxCount++] = wire; }
    ReleaseSRWLockExclusive(&g_aiInboxLock);
}

static void AiTakeInbox(void)
{
    AiWire inbox[32];
    uint64_t from[32];
    AcquireSRWLockExclusive(&g_aiInboxLock);
    uint32_t n = g_aiInboxCount;
    memcpy(inbox, g_aiInbox, sizeof inbox[0] * n);
    memcpy(from, g_aiInboxFrom, sizeof from[0] * n);
    g_aiInboxCount = 0;
    ReleaseSRWLockExclusive(&g_aiInboxLock);
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t speaker = ApiEntityOfNetwork(NULL, inbox[i].network);
        if (speaker == 0xFFFFFFFFu) continue;
        /* The caller's player: the speaker's own player slot, else the pipe's sender, so HF_OWNER_CALLER units obey remote callers. */
        uint64_t peer = 0;
        AiPlayerAvatar(0, &peer, speaker);
        if (!peer) peer = from[i];
        AiHeardOrder(inbox[i].order, speaker, peer, 0, "HellForge's order line");
    }
}

/* ---- units of the rules ---- */

/* The unit settles (AiSettled) after `wait` seconds. */
static void AiPend(uint32_t entity, uint64_t resource, double wait)
{
    for (int i = 0; i < AI_PENDING; i++)
        if (!g_aiPending[i].entity) { g_aiPending[i].entity = entity; g_aiPending[i].resource = resource; g_aiPending[i].at = g_aiClock + wait; return; }
}

static void AiSpawned(HfMod* mod, const HfSpawnEvent* e, void* user)
{
    (void)mod; (void)user;
    if (!e->authority) return;
    for (LONG r = 0; r < g_aiRuleCount; r++)
        if (g_aiRules[r].def.entity == e->resource && !ModGone(g_aiRules[r].mod)) { AiPend(e->entity, e->resource, 1.0); return; }
}

/* The nearest living Helldiver to a point, 0 none. */
/* A Helldiver standing: its own life (0), not the player slot's lifecycle, whose values are not mapped. */
static int AiStanding(uint32_t avatar)
{
    uintptr_t record;
    int32_t health, max;
    uint32_t life = 2;
    return avatar && EvpHealthOf(g_aiGame, avatar, &record, &health, &life, &max) && life == 0;
}

/* A standing Helldiver: the player's (peer), or the avatar given; peerOf gets its player. 0 when none stands. */
static uint32_t AiPlayerAvatar(uint64_t peer, uint64_t* peerOf, uint32_t avatar)
{
    EvpPlayer players[EVT_MAX_PLAYERS];
    int n = EvpPlayers(g_aiGame, players);
    for (int i = 0; i < n; i++)
    {
        if (!players[i].avatar || (peer ? players[i].peer != peer : players[i].avatar != avatar)) continue;
        if (peerOf != NULL) *peerOf = players[i].peer;
        return AiStanding(players[i].avatar) ? players[i].avatar : 0;
    }
    return 0;
}

static uint32_t AiNearestPlayer(const float at[3])
{
    EvpPlayer players[EVT_MAX_PLAYERS];
    int n = EvpPlayers(g_aiGame, players);
    uint32_t best = 0;
    float bestD = 1e30f, p[3];
    for (int i = 0; i < n; i++)
    {
        if (!AiStanding(players[i].avatar)) continue;
        if (AiWhere(players[i].avatar, p) && AiDist(p, at) < bestD) { bestD = AiDist(p, at); best = players[i].avatar; }
    }
    return best;
}

/* The nearest entity of a rider resource within 8 m, 0 none. */
static uint32_t AiRiderOf(uint32_t entity, uint64_t rider, const float pos[3])
{
    HfFind q = {sizeof q, HF_FIND_HOSTILE | HF_FIND_FRIENDLY | HF_FIND_NEUTRAL, 0, 0, {pos[0], pos[1], pos[2]}, 8.0f};
    HfFoundEnemy f[32];
    uint32_t n = rider ? ApiFindEntities(&g_aiMod, &q, f, 32) : 0, best = 0;
    float nearest = 1e30f;
    for (uint32_t i = 0; i < n; i++)
        if (f[i].resource == rider && f[i].entity != entity && f[i].distance < nearest) { nearest = f[i].distance; best = f[i].entity; }
    return best;
}

/* A unit of a rule, its parts set up a second after its spawn (under g_aiLock). */
/* A route of HellForge's own for a unit with no path agent; 0 when routes are refused. */
static uint32_t AiOpenRoute(uint32_t preset)
{
    HfNavProfile profile = {sizeof profile, preset ? preset : HF_NAV_TROOPER, 0, 0, 0, 0};
    return ApiNavRouteOpen(&g_aiMod, &profile);
}

/* A unit with no brain record settles only as a mover's (movers.c), once its mover has read its root. */
static int AiMoverPlace(const AiRule* rule, uint32_t entity, float pos[3])
{
    if (MvWhere(entity, pos)) return 1;
    if (MvPending(entity)) { AiPend(entity, rule->def.entity, 1.0); return 0; } /* its mover's start_delay */
    ApiLog(rule->mod, "ai: unit %u not taken: it has no brain record here, and no mover", entity);
    return 0;
}

static const char* AiHowWalked(const AiUnit* u)
{
    if (AiAgentOf(u->entity)) return "its path agent";
    if (!u->route) return "no path agent and no route";
    return MvWhere(u->entity, (float[3]){0}) ? "no path agent: routed, its mover drives it" : "no path agent: routed, the mod walks it";
}

static void AiStartOrder(AiUnit* u, const AiRule* rule, const float pos[3])
{
    if (rule->def.start_order != HF_ORDER_FOLLOW) return;
    uint32_t player = AiNearestPlayer(pos);
    AiSetOrder(u, HF_ORDER_FOLLOW, player, NULL, "its start order"); /* with nobody standing it waits, then follows */
}

static void AiSettled(uint32_t entity, uint64_t resource)
{
    LONG r = 0;
    while (r < g_aiRuleCount && (g_aiRules[r].def.entity != resource || ModGone(g_aiRules[r].mod))) r++;
    AiCtx c;
    float pos[3];
    if (r == g_aiRuleCount) return;
    const AiRule* rule = &g_aiRules[r];
    const int brain = AiCtxOf(entity, &c) && AiOwnPos(&c, pos);
    if (!brain && !AiMoverPlace(rule, entity, pos)) return;
    if (brain && AiNativeObeys(AiBehaviour(&c))) { ApiLog(rule->mod, "ai: unit %u runs a S.E.A.F. brain, which obeys the wheel itself", entity); return; }
    AiUnit* u = AiUnitOf(entity, 1);
    if (u == NULL) { ApiLog(rule->mod, "ai: unit %u not taken: %d units already obey", entity, AI_UNITS); return; }
    u->mod = rule->mod; u->rule = (uint32_t)r + 1; u->follow = rule->def.follow_distance; u->hold = rule->def.hold_radius;
    u->rider = AiRiderOf(entity, rule->def.rider, pos);
    memcpy(u->lastPos, pos, 12);
    if (!AiAgentOf(entity) && !u->route) u->route = AiOpenRoute(rule->def.nav_preset);
    AiLog(u, "obeys the comms wheel (behaviour %u, %s, rider %u)", brain ? AiBehaviour(&c) : 0, AiHowWalked(u), u->rider);
    AiStartOrder(u, rule, pos);
}

/* ---- friendly fire ---- */

static int AiWantsDamage(void) { return g_aiRuleCount > 0; }

/* Whether a network id is an obeying unit or its rider. Read without g_aiLock: the damage consumer can run inside the
 * unit update that holds it, and a stale id only lets one hit through. */
static AiUnit* AiDealer(uint32_t network)
{
    uint32_t entity = network && network != 0x7FFF ? ApiEntityOfNetwork(&g_aiMod, network) : 0;
    for (int i = 0; entity && i < AI_UNITS; i++)
        if (g_aiUnits[i].rule && (g_aiUnits[i].entity == entity || g_aiUnits[i].rider == entity)) return &g_aiUnits[i];
    return NULL;
}

/* The game removes a stranded obeying unit with a 10000 explosion of its own (kind 4, the unit the dealer). Until what
 * asks for it is known, the hit is spared and logged; the explosion log below names the code that asked. */
static void AiSelfKill(AiUnit* u, uint8_t* event, int32_t amount)
{
    const int32_t none = 0;
    memcpy(event + 0x2C, &none, 4);
    float pos[3] = {0, 0, 0};
    AiWhere(u->entity, pos);
    if (u->selfKills++ < 8)
        AiLog(u, "the game dealt it %d of its own (kind 4) at %.1f %.1f %.1f, %s for %.1f s: spared", amount, (double)pos[0], (double)pos[1],
            (double)pos[2], AiStatusName(u->status), u->status == HF_AI_UNREACHABLE ? AI_RETRY_S - u->retryIn : u->sinceHeadway);
}

/* 1 when the entity is an obeying unit; never waits for the game thread's update. */
static int AiObeying(uint32_t a, uint32_t b)
{
    if (!TryAcquireSRWLockShared(&g_aiLock)) return 0;
    int ours = 0;
    for (int i = 0; i < AI_UNITS && !ours; i++)
        ours = g_aiUnits[i].rule && g_aiUnits[i].entity && (g_aiUnits[i].entity == a || g_aiUnits[i].entity == b);
    ReleaseSRWLockShared(&g_aiLock);
    return ours;
}

/* The caller's return addresses, as game.dll offsets. */
static void AiCallers(char* text, size_t size)
{
    void* frames[12];
    USHORT n = RtlCaptureStackBackTrace(0, 12, frames, NULL);
    uintptr_t game = (uintptr_t)GetModuleHandleA("game.dll");
    text[0] = 0;
    for (USHORT i = 0; i < n; i++)
    {
        uintptr_t at = (uintptr_t)frames[i];
        size_t used = strlen(text);
        int inGame = game && at > game && at - game < 0x5000000u;
        snprintf(text + used, size - used, inGame ? " game+0x%llx" : " ?", (unsigned long long)(at - game));
    }
}

/* Which game code asks for an explosion of an obeying unit. */
static void AiExplosion(uint32_t type, uint32_t source, uint32_t owner, const float* position)
{
    static volatile LONG logged;
    if (g_aiRuleCount == 0 || (source == 0 && owner == 0) || logged >= 16 || !AiObeying(source, owner)) return;
    if (InterlockedIncrement(&logged) > 16) return;
    char text[300];
    AiCallers(text, sizeof text);
    float at[3] = {0, 0, 0};
    if (position != NULL) memcpy(at, position, 12);
    Note("ai: explosion type %u from entity %u (owner %u) at %.1f %.1f %.1f, asked by%s", type, source, owner, (double)at[0],
        (double)at[1], (double)at[2], text);
}

/* An obeying unit's melee ability (damage kind 6: the Scout Strider's kick) on its own side deals nothing. The brain
 * swings it at whatever stands in reach, whatever its side, so a unit that follows at a few metres kicked the Helldiver
 * it followed with no enemy near. Its gunfire and explosions still hit friends in the way, as a sentry's do. */
static void AiDamage(uint8_t* event)
{
    if (g_aiRuleCount == 0) return;
    uint32_t target, kind, dealer, owner;
    int32_t amount;
    memcpy(&kind, event + 4, 4);
    if (kind != 6 && kind != 4) return;
    memcpy(&target, event, 4); memcpy(&dealer, event + 0x10, 4); memcpy(&owner, event + 0x14, 4); memcpy(&amount, event + 0x2C, 4);
    AiUnit* u = AiDealer(dealer);
    if (u == NULL && owner != dealer) u = AiDealer(owner);
    if (u != NULL && kind == 4 && amount >= 10000 && target == u->entity) { AiSelfKill(u, event, amount); return; }
    if (kind != 6) return;
    if (u == NULL || amount == 0 || target == u->entity || target == u->rider) return;
    uint32_t mine = ApiFactionOf(&g_aiMod, u->entity), theirs = ApiFactionOf(&g_aiMod, target);
    if (!(mine & theirs)) return;
    const int32_t none = 0;
    memcpy(event + 0x2C, &none, 4);
    if (u->spared++ == 0) AiLog(u, "its melee hit on entity %u (faction %u, its own side) deals nothing; later ones are not logged", target, theirs);
}

/* ---- the game thread ---- */

static int AiReady(void)
{
    if (g_aiState != 0) return g_aiState > 0;
    if (!g_fired[HF_EVENT_GAME_LOADED]) return 0;
    g_aiGame = ApiModuleBase("game.dll", NULL);
    g_aiMove = (AiMoveFn)ApiFunction(&g_aiMod, "BehaviourMoveTo");
    g_aiStop = (AiStopFn)ApiFunction(&g_aiMod, "BehaviourStop");
    g_aiOwnPos = (AiOwnPosFn)ApiFunction(&g_aiMod, "BehaviourOwnPosition");
    if (!g_aiGame || !g_aiMove || !g_aiStop || !g_aiOwnPos) { g_aiState = -1; Note("ai: orders off; a movement call is not the reviewed one"); return 0; }
    g_aiState = 1;
    return 1;
}

/* The wheel, the game's message and the spawns, once something wants orders. */
static void AiListen(void)
{
    int wanted = g_aiRuleCount > 0 || AnyListener(HF_ON_ORDER);
    /* the spawns separately: an HF_ON_ORDER listener alone starts the rest, and ai_obey may come later */
    if (g_aiRuleCount > 0 && !g_aiSpawnsHeard) { g_aiSpawnsHeard = 1; ApiSubscribe(&g_aiMod, HF_ON_SPAWN, (HfListener)(void (*)(void))AiSpawned, NULL, 0); }
    if (!wanted || g_aiListening) return;
    g_aiListening = 1;
    if (ApiHookFunction(&g_aiMod, "CommsWheelPick", (void*)AiWheelDetour, (void**)&g_aiWheelNext, 0))
        Note("ai: the comms wheel could not be hooked; orders come from the other machines only");
    else g_aiHooked = 1;
    ApiRpcListen(&g_aiMod, AI_RPC_COMMS, AiRpc, NULL, 0);
}

static void AiTick(HfMod* mod, float dt, void* user)
{
    (void)mod; (void)user;
    if (!AiReady()) return;
    AiListen();
    AiTakeInbox();
    g_aiClock += dt;
    if ((g_aiStep += dt) < AI_STEP_S) return;
    g_aiStep = 0;
    AcquireSRWLockExclusive(&g_aiLock);
    for (int i = 0; i < AI_PENDING; i++)
        if (g_aiPending[i].entity && g_aiClock >= g_aiPending[i].at) { AiSettled(g_aiPending[i].entity, g_aiPending[i].resource); g_aiPending[i].entity = 0; }
    for (int i = 0; i < AI_UNITS; i++)
        if (g_aiUnits[i].entity && !AiUnitStep(&g_aiUnits[i])) AiForget(&g_aiUnits[i]);
    ReleaseSRWLockExclusive(&g_aiLock);
}

static void AiStartTick(void);
static void AiStartWanted(void) { if (!g_aiTicking && AnyListener(HF_ON_ORDER)) AiStartTick(); }

static void AiStartTick(void)
{
    if (InterlockedExchange(&g_aiTicking, 1) != 0) return;
    g_aiMod.api = &g_api;
    snprintf(g_aiMod.id, sizeof g_aiMod.id, "hellforge.ai");
    g_aiMod.log = g_log;
    g_aiMod.loaded = 1;
    ApiOnGameThread(&g_aiMod, AiTick, NULL);
}

/* ---- the API ---- */

/* The rule with its defaults filled in, or why it is refused. */
static const char* AiObeyRefusal(const HfAiObey* obey, HfAiObey* d)
{
    if (obey == NULL || obey->size < offsetof(HfAiObey, nav_preset) || obey->entity == 0) return "no HfAiObey, its size or its entity";
    memset(d, 0, sizeof *d);
    memcpy(d, obey, obey->size < sizeof *d ? obey->size : sizeof *d);
    if (d->range == 0) d->range = 60.0f;
    if (d->follow_distance == 0) d->follow_distance = 6.0f;
    if (d->hold_radius == 0) d->hold_radius = 3.0f;
    if (!AiIn(d->range, 1, 500) || !AiIn(d->follow_distance, 1, 50) || !AiIn(d->hold_radius, 0.5f, 50) || d->start_order > HF_ORDER_FOLLOW ||
        d->nav_preset > HF_NAV_VEHICLE_WIDE)
        return "range 1 .. 500, follow_distance 1 .. 50, hold_radius 0.5 .. 50, start_order NONE or FOLLOW, nav_preset 0 .. 3";
    return NULL;
}

static const char* AiAddRule(HfMod* mod, const HfAiObey* d)
{
    const char* why = NULL;
    AcquireSRWLockExclusive(&g_aiLock);
    /* The entity's rule is replaced in its slot when its mod is gone or is this mod again: a live reload runs the
       new version's init while the old one still runs, and its values must win. */
    LONG slot = -1;
    for (LONG i = 0; i < g_aiRuleCount && !why; i++)
    {
        if (g_aiRules[i].def.entity != d->entity) continue;
        if (!ModGone(g_aiRules[i].mod) && strcmp(g_aiRules[i].mod->id, mod->id) != 0) why = "that entity obeys already";
        else if (slot < 0) slot = i;
    }
    if (!why && slot < 0 && g_aiRuleCount >= AI_RULES) why = "32 entities obey already";
    if (!why && slot >= 0) { g_aiRules[slot].mod = mod; g_aiRules[slot].def = *d; }
    else if (!why) { g_aiRules[g_aiRuleCount].mod = mod; g_aiRules[g_aiRuleCount].def = *d; InterlockedIncrement(&g_aiRuleCount); }
    ReleaseSRWLockExclusive(&g_aiLock);
    return why;
}

/* SDK 31, ai_obey: units of an entity obey the comms wheel. 0 done, 1 refused (the mod's log says why). */
static int ApiAiObey(HfMod* mod, const HfAiObey* obey)
{
    HfAiObey d;
    const char* why = AiObeyRefusal(obey, &d);
    if (!why) why = AiAddRule(mod, &d);
    if (why) { ApiLog(mod, "ai_obey: refused: %s", why); return 1; }
    AiStartTick();
    ApiLog(mod, "ai_obey: units of %016llx obey Follow me and Hold position within %.0f m (follow at %.1f m, hold within %.1f m)",
        (unsigned long long)d.entity, (double)d.range, (double)d.follow_distance, (double)d.hold_radius);
    return 0;
}

/* Why ai_order refuses, or NULL (game thread). */
static const char* AiOrderRefusal(uint32_t entity, uint32_t order, uint32_t target, const float point[3])
{
    AiCtx c;
    if (!AiReady()) return g_aiState < 0 ? "the movement calls are not the reviewed ones" : "the game is not loaded yet";
    if (order > HF_ORDER_GOTO || (order == HF_ORDER_FOLLOW && !EvpAlive(target)) || (order == HF_ORDER_GOTO && point == NULL))
        return "an order HF_ORDER_..., FOLLOW with a living target, GOTO with a point";
    if (!EvpAlive(entity)) return "no living unit";
    /* A unit HellForge already routes for an ai_obey rule (a sentry the mod walks) needs no path agent of its own. */
    AcquireSRWLockShared(&g_aiLock);
    const AiUnit* known = AiUnitOf(entity, 0);
    const int routed = known != NULL && (known->rule || known->route);
    ReleaseSRWLockShared(&g_aiLock);
    const int brain = AiCtxOf(entity, &c);
    if (!routed && (!brain || !AiAgentOf(entity))) return "no unit with a brain and a path agent, and not a unit of an ai_obey rule";
    if (brain && !AiAuthority(&c)) return "another machine simulates it (order it there)";
    return NULL;
}

/* SDK 31, ai_order: on the game thread of the machine that simulates the unit. 0 done, 1 refused. */
static int ApiAiOrder(HfMod* mod, uint32_t entity, uint32_t order, uint32_t target, const float point[3])
{
    if (!WcGameThread(mod, "ai_order")) return 1;
    AiStartTick();
    const char* why = AiOrderRefusal(entity, order, target, point);
    if (why) { ApiLog(mod, "ai_order(%u): refused: %s", entity, why); return 1; }
    AcquireSRWLockExclusive(&g_aiLock);
    AiUnit* u = AiUnitOf(entity, order != HF_ORDER_NONE);
    if (u != NULL && !u->rule) u->mod = mod;
    if (u != NULL) AiSetOrder(u, order, target, order == HF_ORDER_FOLLOW ? NULL : point, "the mod");
    if (u != NULL && order == HF_ORDER_NONE && !u->rule) memset(u, 0, sizeof *u);
    ReleaseSRWLockExclusive(&g_aiLock);
    if (u == NULL && order != HF_ORDER_NONE) { ApiLog(mod, "ai_order(%u): refused: %d units have orders already", entity, AI_UNITS); return 1; }
    return 0;
}

/* SDK 31, ai_state: a unit's order and status (HF_AI_IDLE for a unit with none). Any thread. */
static uint32_t ApiAiState(HfMod* mod, uint32_t entity, HfAiState* state)
{
    (void)mod;
    HfAiState s;
    memset(&s, 0, sizeof s);
    s.size = sizeof s;
    AcquireSRWLockShared(&g_aiLock); /* no mod code runs while the update holds it */
    AiUnit* u = AiUnitOf(entity, 0);
    if (u != NULL)
    {
        s.order = u->order; s.status = u->status; s.target = u->target;
        memcpy(s.point, u->detour ? u->detourAt : u->goal, 12);
        memcpy(s.goal, s.point, 12);
        s.steered = u->route != 0;
        if (u->route) AiNextPoint(u, s.point);
        memcpy(s.position, u->lastPos, 12);
        s.distance = AiFlat(u->lastPos, u->point);
        s.moving = (uint32_t)u->moved;
    }
    ReleaseSRWLockShared(&g_aiLock);
    if (g_aiState > 0)
    {
        AiCtx c;
        if (AiCtxOf(entity, &c)) s.behaviour = AiBehaviour(&c);
    }
    if (state != NULL && state->size >= offsetof(HfAiState, steered))
    {
        uint32_t size = state->size < sizeof s ? state->size : (uint32_t)sizeof s;
        memcpy(state, &s, size);
        state->size = size;
    }
    return s.status;
}
