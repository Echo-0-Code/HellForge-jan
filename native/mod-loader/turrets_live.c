/* SDK 41: add_turret's turrets in a mission (hellforge_turret.h's face, docs/modding/turrets.md). Each spawn of a turret
 * entity is logged, and an HF_FACE_AWAY one is turned once it has landed: its root is turned as a mover's is (movers.c),
 * since a thrown sentry otherwise always rests facing the world's +Y. Included by world_calls.c after turrets.c. */

#define TR_UNITS 32
#define TR_STILL_S 1.0  /* resting this long counts as landed */
#define TR_TRIES 3      /* turns written before it gives up (its parent may hold it) */

typedef struct TrUnit
{
    uint32_t entity, def, caller, tries, done;
    float callerAt[3], last[3], want;
    double age, still, sinceTurn;
} TrUnit;

static TrUnit g_trUnits[TR_UNITS];
static SRWLOCK g_trLock = SRWLOCK_INIT;
static volatile LONG g_trTicking;

static HfMod* TrModOf(const TrUnit* t) { return g_trDefs[t->def - 1].mod; }

/* Where it stands and which way its root faces (movers.c's yaw: 0 faces +Y). */
static int TrRoot(const TrUnit* t, uint32_t* unit, float pos[3], float* yaw)
{
    float w[16];
    uint32_t network = 0;
    if (!WcEntity(t->entity, unit, &network) || !ApiJointPose(&g_mvMod, *unit, 0, HF_POSE_WORLD, w) || !isfinite(w[12])) return 0;
    memcpy(pos, w + 12, 12);
    *yaw = atan2f(-w[4], w[5]);
    return 1;
}

/* Its root turned to `t->want` inside its parent's frame, where it stands. */
static int TrTurn(TrUnit* t, uint32_t unit)
{
    MvUnit m;
    float w[16];
    memset(&m, 0, sizeof m);
    m.unit = unit;
    if (!MvFrame(&m, w)) return 0;
    memcpy(m.pos, w + 12, 12);
    m.yaw = t->want;
    MvSetRoot(&m);
    t->tries++;
    t->sinceTurn = 0;
    return 1;
}

static int TrMoverOwns(uint32_t entity)
{
    if (!TryAcquireSRWLockShared(&g_mvLock)) return 1; /* a mover may be turning it: ask again next update */
    const int owned = MvUnitOf(entity) != NULL;
    ReleaseSRWLockShared(&g_mvLock);
    return owned;
}

/* Who called it in: the Helldiver nearest it when it spawns, still in the air over its beacon. */
static void TrCaller(TrUnit* t, const float pos[3])
{
    t->caller = AiNearestPlayer(pos);
    if (t->caller && AiWhere(t->caller, t->callerAt)) return;
    t->caller = 0;
}

/* Once it rests: turn away from the caller, then check the turn held, up to TR_TRIES times. */
static void TrFace(TrUnit* t, uint32_t unit, const float pos[3], float yaw, float dt)
{
    const float dx = pos[0] - t->callerAt[0], dy = pos[1] - t->callerAt[1];
    if (t->tries == 0)
    {
        if (dx * dx + dy * dy < 0.25f) { t->done = 1; ApiLog(TrModOf(t), "add_turret: unit %u landed on its caller: it keeps the game's facing", t->entity); return; }
        t->want = atan2f(-dx, dy);
        TrTurn(t, unit);
        return;
    }
    if ((t->sinceTurn += dt) < 0.5) return;
    const float off = fabsf(MvWrap(yaw - t->want)) * 57.29578f;
    if (off > 5.0f && t->tries < TR_TRIES) { TrTurn(t, unit); return; }
    t->done = 1;
    ApiLog(TrModOf(t), "add_turret: unit %u at %.1f %.1f %.1f faces %.0f deg (world), away from Helldiver %u at %.1f m%s", t->entity,
        (double)pos[0], (double)pos[1], (double)pos[2], (double)(yaw * 57.29578f), t->caller, (double)sqrtf(dx * dx + dy * dy),
        off > 5.0f ? ": the turn did not hold (its parent holds its root)" : "");
}

static void TrUnitTick(TrUnit* t, float dt)
{
    uint32_t unit = 0;
    float pos[3], yaw;
    if (!TrRoot(t, &unit, pos, &yaw)) return;
    if (!t->caller && t->age == 0) TrCaller(t, pos);
    const float dx = pos[0] - t->last[0], dy = pos[1] - t->last[1], dz = pos[2] - t->last[2];
    t->still = dx * dx + dy * dy + dz * dz < 0.0025f ? t->still + dt : 0;
    memcpy(t->last, pos, 12);
    t->age += dt;
    if (t->done || t->still < TR_STILL_S || g_trDefs[t->def - 1].def.face != HF_FACE_AWAY) return;
    if (!t->caller) { t->done = 1; ApiLog(TrModOf(t), "add_turret: unit %u had no Helldiver near when it spawned: it keeps the game's facing", t->entity); return; }
    if (!TrMoverOwns(t->entity)) TrFace(t, unit, pos, yaw, dt);
    else t->done = 1;
}

static void TrTick(HfMod* mod, float dt, void* user)
{
    (void)mod; (void)user;
    if (!MvCalls(dt)) return;
    AcquireSRWLockExclusive(&g_trLock);
    for (int i = 0; i < TR_UNITS; i++)
    {
        TrUnit* t = &g_trUnits[i];
        if (!t->entity) continue;
        if (!EvpAlive(t->entity)) { ApiLog(TrModOf(t), "add_turret: unit %u is gone after %.0f s", t->entity, t->age); memset(t, 0, sizeof *t); continue; }
        TrUnitTick(t, dt);
    }
    ReleaseSRWLockExclusive(&g_trLock);
}

static void TrSpawned(HfMod* mod, const HfSpawnEvent* e, void* user)
{
    (void)mod; (void)user;
    for (LONG d = 0; d < g_trDefCount && d < TR_DEFS; d++)
    {
        if (g_trDefs[d].def.entity != e->resource || ModGone(g_trDefs[d].mod)) continue;
        static const char* faces[] = {"keeps the game's facing", "will face away from its caller"};
        ApiLog(g_trDefs[d].mod, "add_turret: unit %u of %016llx spawned%s; it %s", e->entity, (unsigned long long)e->resource,
            e->authority ? "" : " (another machine simulates it)", faces[g_trDefs[d].def.face]);
        if (!e->authority) return;
        AcquireSRWLockExclusive(&g_trLock);
        for (int i = 0; i < TR_UNITS; i++)
            if (!g_trUnits[i].entity) { memset(&g_trUnits[i], 0, sizeof g_trUnits[i]); g_trUnits[i].entity = e->entity; g_trUnits[i].def = (uint32_t)d + 1; break; }
        ReleaseSRWLockExclusive(&g_trLock);
        return;
    }
}

/* ApiAddTurret, once: the spawn log and the turns run from the first turret on. */
static void TrLiveStart(void)
{
    if (InterlockedExchange(&g_trTicking, 1)) return;
    MvStart(); /* the mover's mod record and teleport calls, which the turns use */
    ApiSubscribe(&g_mvMod, HF_ON_SPAWN, (HfListener)(void (*)(void))TrSpawned, NULL, 0);
    ApiOnGameThread(&g_mvMod, TrTick, NULL);
}
