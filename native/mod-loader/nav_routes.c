/* Navmesh routes for any point, unit or not (SDK 32, docs/modding/ai.md). Included at the end of world_calls.c.
 * A path agent is a handle in an engine interface (game+0x33263D8) that needs no unit, so a mod can have its own.
 * After an ask the corners still hold the old route until the status went 2 (searching) and back to 0 or 3.
 * A nav world goes with its mission: a handle from an earlier state is forgotten, never destroyed.
 * Each call site the slots were read from is checked once; a mismatch turns routes off. Build 25480438. */

#define NAV_AGENTS 0x3326418u  /* [ ]: +0 the nav world (u32) */
#define NAV_API 0x33263D8u     /* [ ]: the path agent interface */
#define NAV_ROUTES 128
#define NAV_NONE 0xFFFFFFFFu
#define NAV_ANSWER_MS 3000u    /* no answer within this: HF_ROUTE_FAILED */

typedef uint32_t (*NavCreateFn)(uint32_t world);
typedef void (*NavHandleFn)(uint32_t h);
typedef void (*NavVecFn)(uint32_t h, const float* p);
typedef void (*NavFloatFn)(uint32_t h, float v);
typedef void (*NavU32Fn)(uint32_t h, uint32_t v);
typedef void (*NavU32x2Fn)(uint32_t h, uint32_t a, uint32_t b);
typedef void (*NavStateFn)(uint32_t h, void* out);
typedef uint32_t (*NavCornersFn)(uint32_t h, uint32_t max, float* out);
typedef uint32_t (*NavHasFn)(uint32_t h);

typedef struct NavRouteSlot
{
    HfMod* mod;
    uint32_t used, handle, world, generation, profile, status, searching, partial;
    uintptr_t api;
    float radius, value;
    float asked[3];
    ULONGLONG askedAt;
} NavRouteSlot;

static NavRouteSlot g_nav[NAV_ROUTES];
static SRWLOCK g_navLock = SRWLOCK_INIT;
static int g_navChecked, g_navOn;
static uintptr_t g_navGame;
static uint32_t g_navGeneration = 1, g_navLastState;

/* Every update (events.c): a new generation whenever the game's state changes, so no handle outlives its mission. */
static void NavRoutesTick(void)
{
    uintptr_t game = g_navGame ? g_navGame : ApiModuleBase("game.dll", NULL);
    uint32_t state = game ? ReadState(game) : HF_STATE_NONE;
    if (state != HF_STATE_NONE && state != g_navLastState) { g_navLastState = state; InterlockedIncrement((volatile LONG*)&g_navGeneration); }
}

/* Once: the call sites the interface's slots and their arguments were read from. */
static int NavCheck(void)
{
    if (g_navChecked) return g_navOn;
    if (!g_fired[HF_EVENT_GAME_LOADED]) return 0;
    g_navChecked = 1;
    g_navGame = ApiModuleBase("game.dll", NULL);
    static const struct { uint32_t rva; const char* code; } pins[] = {
        {0x5B3297u, "48 8B 05 ?? ?? ?? ?? 8B 0E 48 8B 10 FF D2 89 44 3B 08 41 8D 56 20 8B C8 45 8D 46 30 48 8B 05 ?? ?? ?? ?? FF 50 58 "
                    "48 8B 05 ?? ?? ?? ?? 8B 94 3B 9C 00 00 00 8B 4C 3B 08 FF 50 40 48 8B 05 ?? ?? ?? ?? F3 0F 10 8C 3B 94 00 00 00 "
                    "8B 4C 3B 08 FF 50 48 48 8B 05 ?? ?? ?? ?? F3 0F 10 8C 3B 98 00 00 00 8B 4C 3B 08 FF 50 38 48 8B 05 ?? ?? ?? ?? "
                    "BA 88 13 00 00 8B 4C 3B 08"},                                            /* 0x5B31B0: create and set up */
        {0x5B35D9u, "48 8B 05 ?? ?? ?? ?? FF 50 30 48 8B 05 ?? ?? ?? ?? 48 8D 53 0C 8B 4B 08 FF 50 68"}, /* 0x5B34F0: ask, state */
        {0x5B2AFEu, "48 8B 05 ?? ?? ?? ?? 0F 57 F6 48 69 D9 A8 00 00 00 48 8B 50 78 49 03 5B 50 8B 4B 08 FF D2 85 C0 0F 84 ?? ?? ?? ?? "
                    "F6 43 10 0A 0F 84 ?? ?? ?? ?? 8B 43 0C 85 C0 74 09 83 F8 03 0F 85 ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 4C 8D 44 24 20 "
                    "8B 4B 08 BA 40 00 00 00"},                                               /* 0x5B2A70: has a route, corners */
        {0x5B23B5u, "48 8B 05 ?? ?? ?? ?? 48 8D 57 54 48 03 D3 FF 50 28"},                  /* 0x5B2310: position */
        {0x5B1D82u, "48 8B 05 ?? ?? ?? ?? FF 50 08 C7 43 08 FF FF FF FF"},                  /* 0x5B1CD0: destroy */
    };
    for (size_t i = 0; g_navGame && i < sizeof pins / sizeof pins[0]; i++)
    {
        int32_t d = 0;
        uintptr_t site = g_navGame + pins[i].rva;
        if (!ApiCodeMatches(site, pins[i].code) || SafeRead(site + 3, &d, 4) || site + 7 + (intptr_t)d != g_navGame + NAV_API)
        {
            Note("navmesh routes off: the path agent code at 0x%X is not the reviewed one", pins[i].rva);
            return 0;
        }
    }
    g_navOn = g_navGame != 0;
    return g_navOn;
}

/* The interface's slots, each checked to lie in game.dll or the exe; NULL when the interface is not set (no mission). */
static uintptr_t* NavTable(uintptr_t* api, uintptr_t f[20])
{
    if (SafeRead(g_navGame + NAV_API, api, 8) || *api == 0 || SafeRead(*api, f, 20 * sizeof f[0])) return NULL;
    size_t gs = 0, es = 0;
    uintptr_t g = ApiModuleBase("game.dll", &gs), e = ApiModuleBase("helldivers2.exe", &es);
    static const int used[] = {0, 1, 5, 6, 7, 8, 9, 11, 13, 14, 15, 19};
    for (size_t i = 0; i < sizeof used / sizeof used[0]; i++)
    {
        uintptr_t p = f[used[i]];
        if (!((g && p >= g && p < g + gs) || (e && p >= e && p < e + es))) return NULL;
    }
    return f;
}

static uint32_t NavWorld(void)
{
    uintptr_t sys = 0;
    uint32_t world = 0;
    return !SafeRead(g_navGame + NAV_AGENTS, &sys, 8) && sys && !SafeRead(sys, &world, 4) ? world : 0;
}

static NavRouteSlot* NavSlot(HfMod* mod, uint32_t route)
{
    return route >= 1 && route <= NAV_ROUTES && g_nav[route - 1].used && g_nav[route - 1].mod == mod ? &g_nav[route - 1] : NULL;
}

/* The slot's handle in this mission's world, made when missing; 0 when there is none (no mission, refused). */
static int NavHandle(NavRouteSlot* s, uintptr_t* f)
{
    uintptr_t api = 0, table[20];
    if (!NavTable(&api, table)) return 0;
    memcpy(f, table, sizeof table);
    uint32_t world = NavWorld();
    if (s->handle != NAV_NONE && (s->generation != g_navGeneration || s->api != api || s->world != world)) s->handle = NAV_NONE;
    if (s->handle != NAV_NONE) return 1;
    if (!world) return 0;
    s->handle = ((NavCreateFn)f[0])(world);
    if (s->handle == NAV_NONE) return 0;
    s->api = api; s->world = world; s->generation = g_navGeneration;
    ((NavU32x2Fn)f[11])(s->handle, 0x20, 0x30); /* as the game sets its own up (0x5B31B0) */
    ((NavU32Fn)f[8])(s->handle, s->profile);
    ((NavFloatFn)f[9])(s->handle, s->value);
    ((NavFloatFn)f[7])(s->handle, s->radius);
    ((NavU32Fn)f[19])(s->handle, 5000);
    s->status = HF_ROUTE_NONE;
    return 1;
}

static const char* NavProfileRefusal(HfMod* mod, const HfNavProfile* p, uint32_t* profile, float* radius, float* value)
{
    if (p == NULL || p->size < sizeof *p) return "no HfNavProfile, or its size is not set";
    switch (p->preset)
    {
    case HF_NAV_TROOPER: *profile = 0xC8A0026Fu; *radius = 0.79f; *value = 0.5f; return NULL;
    case HF_NAV_VEHICLE: *profile = 0x76096092u; *radius = 1.05f; *value = 0.5f; return NULL;
    case HF_NAV_VEHICLE_WIDE: *profile = 0x76096092u; *radius = 2.10f; *value = 40.0f; return NULL;
    case 0: break;
    default: return "an unknown preset";
    }
    if (p->entity)
    {
        typedef const uint8_t* (*NavRecordFn)(uint64_t resource);
        NavRecordFn record = (NavRecordFn)ApiFunction(mod, "NavigationRecord");
        const uint8_t* r = record ? record(p->entity) : NULL;
        if (r == NULL || SafeRead((uintptr_t)r, value, 4) || SafeRead((uintptr_t)r + 4, profile, 4) || SafeRead((uintptr_t)r + 8, radius, 4) || !*profile)
            return "the entity has no Navigation record (or the game has not loaded it yet)";
        *radius = (*radius > 0.0f ? fminf(*radius, 2.4f) : 0.5f) * 1.05f; /* the game's own sizing (0x5B1C40) */
        return NULL;
    }
    if (!p->profile || !(p->radius > 0.0f) || p->radius > 2.6f) return "a preset, an entity, or a profile hash with a radius in (0, 2.6]";
    *profile = p->profile; *radius = p->radius; *value = p->value > 0.0f ? p->value : 0.5f;
    return NULL;
}

/* SDK 32, nav_route_open: a route of the mod's own. Its id, 0 refused (the log says why). Any thread. */
static uint32_t ApiNavRouteOpen(HfMod* mod, const HfNavProfile* p)
{
    uint32_t profile = 0;
    float radius = 0, value = 0;
    const char* why = NavProfileRefusal(mod, p, &profile, &radius, &value);
    if (why) { ApiLog(mod, "nav_route_open: refused: %s", why); return 0; }
    AcquireSRWLockExclusive(&g_navLock);
    uint32_t id = 0;
    for (uint32_t i = 0; i < NAV_ROUTES && !id; i++)
        if (!g_nav[i].used)
        {
            memset(&g_nav[i], 0, sizeof g_nav[i]);
            g_nav[i].used = 1; g_nav[i].mod = mod; g_nav[i].handle = NAV_NONE;
            g_nav[i].profile = profile; g_nav[i].radius = radius; g_nav[i].value = value;
            id = i + 1;
        }
    ReleaseSRWLockExclusive(&g_navLock);
    if (!id) ApiLog(mod, "nav_route_open: refused: all %d routes are open", NAV_ROUTES);
    return id;
}

/* SDK 32, nav_route_ask: a route from `from` to `to` (to is put on the ground under or over it first). Game thread, in
 * a mission. 0 asked, 1 refused. */
static int ApiNavRouteAsk(HfMod* mod, uint32_t route, const float from[3], const float to[3])
{
    if (!WcGameThread(mod, "nav_route_ask")) return 1;
    if (from == NULL || to == NULL || !NavCheck()) return 1;
    AcquireSRWLockExclusive(&g_navLock);
    NavRouteSlot* s = NavSlot(mod, route);
    uintptr_t f[20];
    int ok = s != NULL && NavHandle(s, f);
    if (ok)
    {
        float goal[3] = {to[0], to[1], to[2]};
        HfSight how = {sizeof how};
        HfSightHit hit;
        memset(&hit, 0, sizeof hit);
        const float top[3] = {to[0], to[1], to[2] + 40.0f}, bottom[3] = {to[0], to[1], to[2] - 40.0f};
        if (ApiLineOfSight(mod, top, bottom, &how, &hit) == HF_SIGHT_BLOCKED && hit.unit == 0 && isfinite(hit.position[2])) goal[2] = hit.position[2];
        ((NavVecFn)f[5])(s->handle, from);
        ((NavVecFn)f[6])(s->handle, goal);
        memcpy(s->asked, goal, 12);
        s->askedAt = GetTickCount64();
        s->status = HF_ROUTE_SEARCHING;
        s->searching = 0;
        uint8_t st[0x80] = {0};
        ((NavStateFn)f[13])(s->handle, st);
        if (st[0] == 2) s->searching = 1;
    }
    ReleaseSRWLockExclusive(&g_navLock);
    return ok ? 0 : 1;
}

/* A searching route's answer, once the engine has gone through searching (status 2) since the ask. */
static void NavPoll(NavRouteSlot* s, const uintptr_t* f)
{
    uint8_t st[0x80] = {0};
    uint32_t status, flags;
    ((NavStateFn)f[13])(s->handle, st);
    memcpy(&status, st, 4); memcpy(&flags, st + 4, 4);
    if (status == 2) { s->searching = 1; return; }
    if (s->searching && (status == 0 || status == 3) && (flags & 0xAu) && ((NavHasFn)f[15])(s->handle))
    { s->status = HF_ROUTE_READY; s->partial = !(flags & 8u); }
    else if (GetTickCount64() - s->askedAt > NAV_ANSWER_MS) s->status = HF_ROUTE_FAILED;
}

/* A ready route's corners into r (and corners); PARTIAL when it ends over a metre from the goal. */
static void NavCorners(const NavRouteSlot* s, const uintptr_t* f, HfNavRoute* r, float* corners, uint32_t max)
{
    static float pts[64 * 3];
    uint32_t n = ((NavCornersFn)f[14])(s->handle, 64, pts);
    if (n > 64) n = 64;
    for (uint32_t i = 1; i < n; i++)
    {
        float x = pts[3 * i] - pts[3 * i - 3], y = pts[3 * i + 1] - pts[3 * i - 2], z = pts[3 * i + 2] - pts[3 * i - 1];
        r->length += sqrtf(x * x + y * y + z * z);
    }
    r->count = n;
    if (n) memcpy(r->end, pts + 3 * (n - 1), 12);
    float ex = r->end[0] - s->asked[0], ey = r->end[1] - s->asked[1];
    if (s->partial || n == 0 || sqrtf(ex * ex + ey * ey) > 1.0f) r->status = HF_ROUTE_PARTIAL;
    if (corners && max) memcpy(corners, pts, (size_t)(n < max ? n : max) * 12);
}

/* The route's state into r, under g_navLock. */
static void NavRead(HfMod* mod, uint32_t route, const float from[3], HfNavRoute* r, float* corners, uint32_t max)
{
    NavRouteSlot* s = NavSlot(mod, route);
    uintptr_t f[20];
    if (s == NULL) return;
    if (s->handle == NAV_NONE || s->generation != g_navGeneration || !NavHandle(s, f)) { r->status = HF_ROUTE_NONE; return; }
    if (from) ((NavVecFn)f[5])(s->handle, from);
    if (s->status == HF_ROUTE_SEARCHING) NavPoll(s, f);
    r->status = s->status;
    memcpy(r->goal, s->asked, 12);
    if (s->status == HF_ROUTE_READY) NavCorners(s, f, r, corners, max);
}

/* SDK 32, nav_route_read: how the route stands (HF_ROUTE_...). Game thread. */
static uint32_t ApiNavRouteRead(HfMod* mod, uint32_t route, const float from[3], HfNavRoute* out, float* corners, uint32_t max)
{
    HfNavRoute r = {sizeof r, HF_ROUTE_OFF, 0, 0.0f, {0, 0, 0}, {0, 0, 0}};
    if (WcGameThread(mod, "nav_route_read") && NavCheck())
    {
        AcquireSRWLockExclusive(&g_navLock);
        NavRead(mod, route, from, &r, corners, max);
        ReleaseSRWLockExclusive(&g_navLock);
    }
    if (out && out->size >= sizeof r) *out = r;
    return r.status;
}

/* SDK 32, nav_route_close: the route's agent goes (destroyed in its own mission, else forgotten). Game thread. */
static void ApiNavRouteClose(HfMod* mod, uint32_t route)
{
    if (!WcGameThread(mod, "nav_route_close")) return;
    AcquireSRWLockExclusive(&g_navLock);
    NavRouteSlot* s = NavSlot(mod, route);
    uintptr_t api = 0, f[20];
    if (s != NULL && s->handle != NAV_NONE && g_navOn && s->generation == g_navGeneration && NavTable(&api, f) && api == s->api && NavWorld() == s->world)
        ((NavHandleFn)f[1])(s->handle);
    if (s != NULL) memset(s, 0, sizeof *s);
    ReleaseSRWLockExclusive(&g_navLock);
}
