/* SDK 28 (hellforge_world.h): line of sight, projectiles near a point and sides, for game.dll and helldivers2.exe build
 * 25480438. Where each one reads and what it calls: docs/internals-world.md. Included by world_calls.c after its
 * find_enemies scan (WcScan), which find_entities shares. */

/* ---- line_of_sight ------------------------------------------------------------------------------------------------ */

#define WS_PHYSICS_WORLD 0x346BFA0u /* game.dll: the physics world; the exe's world id call proves it registered */
#define WS_AI_RAYS 0x347D7E0u       /* game.dll: the AI's raycast batch; its task rows at +0x40008 */
#define WS_AI_RAY_ROWS 24u
#define WS_SCENES 0x27BA870u        /* exe: a world id's physics scene at +0x20, 0xB0 a world */
#define WS_RAY_HITS 8u
#define WS_HIT_SIZE 44u
#define WS_HIT_DISTANCE 24u         /* then the unit hit at +28 */
/* The game's own preset for this filter (exe+0x27B9440 entry 1): mode 2, 5, then the filter's two words. */
#define WS_RAY_A 2u
#define WS_RAY_B 5u
#define WS_RAY_C 0x393D9518u
#define WS_RAY_FILTER 0x80000009u

typedef uint32_t (*WsWorldIdFn)(uintptr_t world);
typedef uint32_t (*WsRaycastFn)(uint32_t world, const float* from, const float* dir, float length, uint32_t a, uint32_t b,
    uint32_t c, uint32_t filter, uint32_t ignore, void* hits, uint32_t max);

/* The exe's world id and raycast calls, and the AI task's read of its batch (so the batch is where we look). */
static const ActPin kWsSightPins[] = {{1, 0x79F860u, "33c04c8d05"}, {1, 0x79F869u, "0f1f800000000049390cc0"},
    {1, 0x7F4590u, "488bc4f30f115820894808555356574154415541564157"}, {0, 0x175C5D6u, "488b3d0312d201"}};
static int g_wsSight; /* 0 not checked, 1 as reviewed, -1 not */

static int WsSightReady(void)
{
    uintptr_t game = ApiModuleBase("game.dll", NULL);
    if (g_wsSight == 0 && game) g_wsSight = ActPins(game, kWsSightPins, sizeof kWsSightPins / sizeof kWsSightPins[0]) ? 1 : -1;
    return g_wsSight > 0;
}

/* Whether the AI's raycast tasks are all done: the raycast shares their scene, and queued rays don't matter (they
 * wait for the next kick), only rows still running. */
static int WsAiRaysIdle(uintptr_t game)
{
    uintptr_t batch;
    uint32_t rows[WS_AI_RAY_ROWS * 3];
    if (!EvpPtr(game + WS_AI_RAYS, &batch) || SafeRead(batch + 0x40008, rows, sizeof rows)) return 0;
    for (uint32_t i = 0; i < WS_AI_RAY_ROWS; i++) if (rows[3 * i + 2] != 1) return 0;
    return 1;
}

/* The physics world's id, or ~0 with why: the raycast calls through its scene unchecked, so a world with none crashes. */
static uint32_t WsWorld(uintptr_t game, const char** why)
{
    uintptr_t exe = (uintptr_t)GetModuleHandleW(NULL), world, scene;
    if (!EvpPtr(game + WS_PHYSICS_WORLD, &world)) { *why = "there is no physics world now"; return ~0u; }
    uint32_t id = ((WsWorldIdFn)(exe + 0x79F860u))(world);
    if (id >= 4 || !EvpPtr(exe + WS_SCENES + 0xB0u * id + 0x20, &scene)) { *why = "the physics world has no scene now"; return ~0u; }
    return id;
}

static int WsIgnored(const HfSight* how, uint32_t unit)
{
    for (int i = 0; how && unit && i < 4; i++) if (how->ignore[i] == unit) return 1;
    return 0;
}

/* Why a ray can't be cast now, or NULL with *id the physics world's. */
static const char* WsSightWhyNot(uintptr_t game, const float* from, const float* to, const HfSight* how, uint32_t* id)
{
    const char* why = NULL;
    if (from == NULL || to == NULL) return "no from or to";
    if (how && how->size < sizeof *how) return "its HfSight's size is not set";
    if (g_gameThread == 0 || GetCurrentThreadId() != g_gameThread) return "call it on the game thread";
    if (!game || ReadState(game) != HF_STATE_MISSION) return "not in a mission";
    if (!WsSightReady()) return "the engine's raycast is not the reviewed one";
    if (!WsAiRaysIdle(game)) return "the AI's own rays are being cast (ask again next update)";
    *id = WsWorld(game, &why);
    return why;
}

/* The nearest of n hits that counts (the engine does not sort them): 1 with *hit filled (when not NULL). */
static int WsFirstHit(const uint8_t* hits, uint32_t n, const float* from, const float* dir, float range, const HfSight* how, HfSightHit* hit)
{
    float skipFrom = how ? how->skip_from : 0, skipTo = how ? how->skip_to : 0, best = -1;
    uint32_t bestUnit = 0;
    for (uint32_t h = 0; h < n && h < WS_RAY_HITS; h++)
    {
        float d;
        uint32_t unit;
        memcpy(&d, hits + h * WS_HIT_SIZE + WS_HIT_DISTANCE, 4);
        memcpy(&unit, hits + h * WS_HIT_SIZE + WS_HIT_DISTANCE + 4, 4);
        if (!isfinite(d) || d < skipFrom || d > range - skipTo || WsIgnored(how, unit) || (best >= 0 && d >= best)) continue;
        best = d;
        bestUnit = unit == 0xFFFFFFFFu ? 0 : unit;
    }
    if (best >= 0 && hit) *hit = (HfSightHit){best, bestUnit, {from[0] + dir[0] * best, from[1] + dir[1] * best, from[2] + dir[2] * best}};
    return best >= 0;
}

/* SDK 28, line_of_sight: HF_SIGHT_CLEAR, HF_SIGHT_BLOCKED (hit filled) or HF_SIGHT_UNKNOWN (logged, at most 8 times). */
static uint32_t ApiLineOfSight(HfMod* mod, const float from[3], const float to[3], const HfSight* how, HfSightHit* hit)
{
    static volatile LONG logged;
    uint32_t id = ~0u;
    const char* why = WsSightWhyNot(ApiModuleBase("game.dll", NULL), from, to, how, &id);
    if (why)
    {
        if (InterlockedIncrement(&logged) <= 8) ApiLog(mod, "line_of_sight: not known: %s", why);
        return HF_SIGHT_UNKNOWN;
    }
    float dir[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    float range = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (!isfinite(range) || range < 1e-3f || (how && range <= how->skip_from + how->skip_to)) return HF_SIGHT_CLEAR;
    for (int k = 0; k < 3; k++) dir[k] /= range;
    uint8_t hits[WS_RAY_HITS * WS_HIT_SIZE];
    memset(hits, 0, sizeof hits);
    uintptr_t exe = (uintptr_t)GetModuleHandleW(NULL);
    uint32_t n = ((WsRaycastFn)(exe + 0x7F4590u))(id, from, dir, range, WS_RAY_A, WS_RAY_B, WS_RAY_C, WS_RAY_FILTER,
        how ? how->ignore[0] : 0, hits, WS_RAY_HITS);
    return WsFirstHit(hits, n, from, dir, range, how, hit) ? HF_SIGHT_BLOCKED : HF_SIGHT_CLEAR;
}

/* ---- find_projectiles and set_projectile ---------------------------------------------------------------------------- */

/* The projectile pool ([game + 0x347CEA8], as actions.c reads it): +0x28 running; per slot of 2048 a u16 flag (+0x203C:
 * bit 1 in use, bit 0 ended), a 0x70 flight record (+0x3040: position, velocity +0xC), a 0x24 source record (+0x3B040:
 * the entity at +0xC), a 0xC8 hit record (+0x4D040: the credited peer at +0, impact requested at +0xB9), its type
 * (+0xE5040). */
#define WS_POOL 0x347CEA8u
#define WS_POOL_SLOTS 2048u
static const ActPin kWsPoolPins[] = {{0, 0x5F5E03u, "488b0d9e70e8024c897500c7"}, {0, 0x612F0Bu, "488b05969fe602f20f11b5e0"}};
static int g_wsPool;
static HfFoundProjectile g_wsShots[WS_POOL_SLOTS];
static SRWLOCK g_wsShotsLock = SRWLOCK_INIT;

static uintptr_t WsPool(uintptr_t game)
{
    uintptr_t pool;
    uint8_t running = 0;
    if (g_wsPool == 0) g_wsPool = ActPins(game, kWsPoolPins, sizeof kWsPoolPins / sizeof kWsPoolPins[0]) ? 1 : -1;
    if (g_wsPool < 0 || ReadState(game) != HF_STATE_MISSION || !EvpPtr(game + WS_POOL, &pool) || SafeRead(pool + 0x28, &running, 1) ||
        running != 1) return 0;
    return pool;
}

/* A slot's shot while it flies: 1 with position and velocity, 0 when the slot is free, ended or has hit. */
static int WsShot(uintptr_t pool, uint32_t slot, uint32_t* type, float f[6])
{
    uint16_t flags = 0;
    uint8_t requested = 1;
    if (SafeRead(pool + 0x203C + (uintptr_t)slot * 2, &flags, 2) || (flags & 3) != 2 ||
        SafeRead(pool + 0x4D040 + (uintptr_t)slot * 0xC8 + 0xB9, &requested, 1) || requested ||
        SafeRead(pool + 0x3040 + (uintptr_t)slot * 0x70, f, 6 * sizeof f[0])) return 0;
    for (int k = 0; k < 6; k++) if (!isfinite(f[k]) || fabsf(f[k]) > 100000.0f) return 0;
    *type = EvpU32(pool + 0xE5040 + (uintptr_t)slot * 4, 0);
    return 1;
}

static int WsNearer(const void* a, const void* b)
{
    float x = ((const HfFoundProjectile*)a)->distance, y = ((const HfFoundProjectile*)b)->distance;
    return x < y ? -1 : x > y;
}

/* Slot s's shot as an answer, when it is of `type` (0: any) and within radius of centre: 1 with *p filled. */
static int WsFoundAt(uintptr_t pool, uint32_t s, const float* centre, float radius, uint32_t type, HfFoundProjectile* p)
{
    uint32_t t;
    float f[6];
    if (!WsShot(pool, s, &t, f) || (type && t != type)) return 0;
    float dx = f[0] - centre[0], dy = f[1] - centre[1], dz = f[2] - centre[2], d = sqrtf(dx * dx + dy * dy + dz * dz);
    if (!(d <= radius)) return 0;
    memset(p, 0, sizeof *p);
    p->slot = s;
    p->type = t;
    memcpy(p->position, f, 12);
    memcpy(p->velocity, f + 3, 12);
    p->distance = d;
    p->source = EvpU32(pool + 0x3B040 + (uintptr_t)s * 0x24 + 0x0C, 0);
    if (p->source == 0xFFFFFFFFu) p->source = 0;
    SafeRead(pool + 0x4D040 + (uintptr_t)s * 0xC8, &p->peer, 8);
    return 1;
}

/* SDK 28, find_projectiles: how many shots were written to out, nearest first. */
static uint32_t ApiFindProjectiles(HfMod* mod, const float centre[3], float radius, uint32_t type, HfFoundProjectile* out, uint32_t capacity)
{
    uintptr_t game = ApiModuleBase("game.dll", NULL), pool;
    if (centre == NULL || (capacity > 0 && out == NULL) || !(radius > 0.0f)) { ApiLog(mod, "find_projectiles: refused: no centre, no room or no radius"); return 0; }
    if (!game || !(pool = WsPool(game))) return 0;
    AcquireSRWLockExclusive(&g_wsShotsLock);
    uint32_t found = 0;
    for (uint32_t s = 0; s < WS_POOL_SLOTS; s++) found += (uint32_t)WsFoundAt(pool, s, centre, radius, type, &g_wsShots[found]);
    qsort(g_wsShots, found, sizeof g_wsShots[0], WsNearer);
    uint32_t written = found < capacity ? found : capacity;
    if (written) memcpy(out, g_wsShots, sizeof out[0] * written);
    ReleaseSRWLockExclusive(&g_wsShotsLock);
    return written;
}

/* SDK 28, set_projectile: 1 done, 0 refused (logged). */
static int ApiSetProjectile(HfMod* mod, uint32_t slot, uint32_t type, const float position[3], const float velocity[3])
{
    uintptr_t game = ApiModuleBase("game.dll", NULL), pool;
    uint32_t t;
    float f[6];
    if (!WcGameThread(mod, "set_projectile")) return 0;
    if (slot >= WS_POOL_SLOTS || (position == NULL && velocity == NULL)) { ApiLog(mod, "set_projectile: refused: no such slot, or nothing to set"); return 0; }
    for (int k = 0; k < 3; k++)
        if ((position && !(fabsf(position[k]) <= 100000.0f)) || (velocity && !(fabsf(velocity[k]) <= 5000.0f)))
        { ApiLog(mod, "set_projectile: refused: a position or velocity that is not finite or not plausible"); return 0; }
    if (!game || !(pool = WsPool(game)) || !WsShot(pool, slot, &t, f) || t != type) return 0; /* it hit or ended: nothing to move */
    uintptr_t flight = pool + 0x3040 + (uintptr_t)slot * 0x70;
    if (position) memcpy((void*)flight, position, 12);
    if (velocity) memcpy((void*)(flight + 0x0C), velocity, 12);
    return 1;
}

/* ---- faction_of, side_of and find_entities ------------------------------------------------------------------------- */

/* The side of a mask seen from `toward`, as the game's perception sorts it (docs/internals-world.md); seen from the
 * Helldivers, an enemy type with no mask counts as hostile, as find_enemies has always counted it. */
static uint32_t WsSide(uint32_t mask, uint32_t toward, uint64_t resource)
{
    if (toward == 0) toward = HF_FACTION_HELLDIVERS;
    if (mask == 0) return (toward & HF_FACTION_HELLDIVERS) && ActEnemy(resource) ? HF_SIDE_HOSTILE : HF_SIDE_NEUTRAL;
    return (mask & toward) ? HF_SIDE_FRIENDLY : HF_SIDE_HOSTILE;
}

/* SDK 28, faction_of: the entity's mask, 0 for none. */
static uint32_t ApiFactionOf(HfMod* mod, uint32_t entity)
{
    (void)mod;
    uintptr_t game = ApiModuleBase("game.dll", NULL);
    WcScan s;
    if (!game || !entity || !EvpAlive(entity) || !WcScanOf(game, &s)) return 0;
    return WcFaction(&s, entity);
}

/* SDK 28, side_of: HF_SIDE_..., HF_SIDE_UNKNOWN when it does not read. */
static uint32_t ApiSideOf(HfMod* mod, uint32_t entity, uint32_t toward)
{
    (void)mod;
    uintptr_t game = ApiModuleBase("game.dll", NULL);
    WcScan s;
    if (!game || !entity || !EvpAlive(entity) || !WcScanOf(game, &s) || !s.factionsOk) return HF_SIDE_UNKNOWN;
    return WsSide(WcFaction(&s, entity), toward, EvpEntityResource(game, entity, NULL));
}
