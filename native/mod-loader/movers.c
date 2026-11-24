/* SDK 35: movers (docs/modding/movers.md, hellforge_move.h). A mover's unit is moved by HellForge on the host: its
 * root node is teleported each update, inside the frame of the unit it is parented to (a hellpod), from a throttle and
 * a steer. The ground is found with rays (line_of_sight); the root's moves replicate as a sentry's do.
 * Included by world_calls.c after ai_orders.c (AiDrivePoint, ApiAiObey). */

#define MV_DEFS 32
#define MV_UNITS 64
#define MV_INPUT_S 0.5     /* a mod's input holds this long */
#define MV_SOLID_S 0.5     /* how often the moving units near a mover are listed */

typedef void (*MvPosFn)(uint32_t unit, int32_t node, const float* position);
typedef void* (*MvLevelFn)(uint32_t unit);

typedef struct MvDef { HfMod* mod; HfMoverDef def; int ai; float arrive; } MvDef;
typedef struct MvUnit
{
    uint32_t entity, unit, def, started, status, driver;
    float pos[3], yaw, speed, turning, throttle, steer, par[4][3], parYaw;
    double age, inputAge, sinceSolid;
    uint32_t mobile[32], mobileCount, blockedLogged;
} MvUnit;

static MvDef g_mvDefs[MV_DEFS];
static volatile LONG g_mvDefCount;
static MvUnit g_mvUnits[MV_UNITS];
static SRWLOCK g_mvLock = SRWLOCK_INIT;
static HfMod g_mvMod;
static volatile LONG g_mvTicking;
static MvPosFn g_mvPos, g_mvRot;
static MvLevelFn g_mvLevel;

static float MvWrap(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static float MvClamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static MvUnit* MvUnitOf(uint32_t entity)
{
    for (int i = 0; i < MV_UNITS; i++) if (entity && g_mvUnits[i].entity == entity) return &g_mvUnits[i];
    return NULL;
}

/* ---- the ground ---- */

/* A hit unit that is solid: the level's own (rocks, props), or anything that is not a moving unit listed around it. */
static int MvSolid(const MvUnit* m, uint32_t unit)
{
    if (g_mvLevel && g_mvLevel(unit) != NULL) return 1;
    for (uint32_t i = 0; i < m->mobileCount; i++) if (m->mobile[i] == unit) return 0;
    return 1;
}

/* The first solid hit on the ray, moving units passed (up to three). 1 with its height in *z. */
static int MvRay(const MvUnit* m, const float from[3], const float to[3], float* z)
{
    HfSight how = {sizeof how, {m->unit, 0, 0, 0}, 0, 0};
    for (int k = 1; k <= 4; k++)
    {
        HfSightHit hit;
        memset(&hit, 0, sizeof hit);
        if (ApiLineOfSight(&g_mvMod, from, to, &how, &hit) != HF_SIGHT_BLOCKED || !isfinite(hit.position[2])) return 0;
        if (hit.unit == 0 || MvSolid(m, hit.unit)) { *z = hit.position[2]; return 1; }
        if (k < 4) how.ignore[k] = hit.unit;
    }
    return 0;
}

/* The ground at (x, y) for a unit at height z: within its steps 1, too high or too low 0. No ground in reach keeps
 * its height, or drops it to ground far below (it walked off an edge), or lifts it onto ground above (it sank). */
static int MvGround(const MvUnit* m, const HfMoverDef* d, float x, float y, float z, float* out)
{
    float g;
    const float top[3] = {x, y, z + 1.2f}, low[3] = {x, y, z - 3.0f}, under[3] = {x, y, z + d->step_up + 0.02f};
    if (MvRay(m, top, low, &g) && g - z > d->step_up && !MvRay(m, under, low, &g)) return 0; /* a block: nothing under it */
    if (MvRay(m, g - z > d->step_up ? under : top, low, &g))
    {
        if (g - z > d->step_up || z - g > d->step_down) return 0;
        *out = g;
        return 1;
    }
    const float below[3] = {x, y, z - 1.0f}, deep[3] = {x, y, z - 80.0f}, sky[3] = {x, y, z + 80.0f}, here[3] = {x, y, z};
    if (MvRay(m, below, deep, &g)) { *out = g; return 1; }
    if (MvRay(m, sky, here, &g) && g > z + 0.5f) { *out = g; return 1; }
    *out = z;
    return 1;
}

/* A wall ahead: a ray just over its highest step, from its root to reach past the step. */
static int MvWall(const MvUnit* m, const HfMoverDef* d, float ux, float uy, float step)
{
    const float h = m->pos[2] + d->step_up + 0.05f, reach = step + d->reach;
    const float from[3] = {m->pos[0], m->pos[1], h}, to[3] = {m->pos[0] + ux * reach, m->pos[1] + uy * reach, h};
    float z;
    return MvRay(m, from, to, &z);
}

/* One step of `step` metres (negative: backward) along its heading; a walker also tries 35, 70 and 105 degrees to either
 * side. 1 moved. */
static int MvStep(MvUnit* m, const HfMoverDef* d, float step)
{
    static const float kTry[] = {0.0f, 0.61f, -0.61f, 1.22f, -1.22f, 1.83f, -1.83f};
    const unsigned tries = d->mode == HF_MOVE_WALK && step > 0 ? 7u : 1u;
    const float fx = -sinf(m->yaw), fy = cosf(m->yaw), sign = step < 0 ? -1.0f : 1.0f, len = fabsf(step);
    for (unsigned i = 0; i < tries; i++)
    {
        const float c = cosf(kTry[i]), s = sinf(kTry[i]);
        const float ux = (fx * c - fy * s) * sign, uy = (fx * s + fy * c) * sign;
        float g;
        if (MvWall(m, d, ux, uy, len) || !MvGround(m, d, m->pos[0] + ux * len, m->pos[1] + uy * len, m->pos[2], &g)) continue;
        m->pos[0] += ux * len; m->pos[1] += uy * len; m->pos[2] = g;
        return 1;
    }
    return 0;
}

/* The moving units around it (Helldivers, enemies, other movers), which its rays pass. */
static void MvListMobile(MvUnit* m)
{
    HfFind q = {sizeof q, HF_FIND_HOSTILE | HF_FIND_FRIENDLY | HF_FIND_NEUTRAL, 0, 0, {m->pos[0], m->pos[1], m->pos[2]}, 20.0f};
    HfFoundEnemy f[32];
    const uint32_t n = ApiFindEntities(&g_mvMod, &q, f, 32);
    m->mobileCount = 0;
    for (uint32_t i = 0; i < n && i < 32; i++) if (f[i].unit) m->mobile[m->mobileCount++] = f[i].unit;
}

/* ---- the root ---- */

/* The root's parent frame from its world and local poses (rows right, forward, up), so moves are written in it. */
static int MvParent(MvUnit* m)
{
    float w[16], l[16], wn[9], ln[9];
    if (!ApiJointPose(&g_mvMod, m->unit, 0, HF_POSE_WORLD, w) || !ApiJointPose(&g_mvMod, m->unit, 0, HF_POSE_LOCAL, l)) return 0;
    const HfJointPose* L = (const HfJointPose*)(const void*)l;
    for (int i = 0; i < 3; i++)
    {
        const float a = sqrtf(w[4 * i] * w[4 * i] + w[4 * i + 1] * w[4 * i + 1] + w[4 * i + 2] * w[4 * i + 2]);
        const float b = sqrtf(L->axes[3 * i] * L->axes[3 * i] + L->axes[3 * i + 1] * L->axes[3 * i + 1] + L->axes[3 * i + 2] * L->axes[3 * i + 2]);
        if (!(a > 1e-4f) || !(b > 1e-4f) || !isfinite(w[12])) return 0;
        for (int j = 0; j < 3; j++) { wn[3 * i + j] = w[4 * i + j] / a; ln[3 * i + j] = L->axes[3 * i + j] / b; }
    }
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) m->par[i][j] = ln[i] * wn[j] + ln[3 + i] * wn[3 + j] + ln[6 + i] * wn[6 + j];
    for (int j = 0; j < 3; j++)
        m->par[3][j] = w[12 + j] - (L->position[0] * m->par[0][j] + L->position[1] * m->par[1][j] + L->position[2] * m->par[2][j]);
    m->parYaw = atan2f(-m->par[1][0], m->par[1][1]);
    memcpy(m->pos, w + 12, 12);
    m->yaw = atan2f(-w[4], w[5]);
    return 1;
}

static void MvSetRoot(const MvUnit* m)
{
    const float d[3] = {m->pos[0] - m->par[3][0], m->pos[1] - m->par[3][1], m->pos[2] - m->par[3][2]};
    float local[3];
    for (int i = 0; i < 3; i++) local[i] = d[0] * m->par[i][0] + d[1] * m->par[i][1] + d[2] * m->par[i][2];
    const float h = (m->yaw - m->parYaw) * 0.5f, q[4] = {0.0f, 0.0f, sinf(h), cosf(h)};
    g_mvPos(m->unit, 0, local);
    g_mvRot(m->unit, 0, q);
}

/* ---- driving ---- */

/* The AI's inputs toward its next route point, as the mover's own turning allows. */
static void MvAiInputs(MvUnit* m, const MvDef* def)
{
    float point[3];
    uint32_t status = 0;
    m->throttle = 0; m->steer = 0;
    if (!AiDrivePoint(m->entity, point, &status) || (status != HF_AI_MOVING && status != HF_AI_DETOUR)) return;
    const float dx = point[0] - m->pos[0], dy = point[1] - m->pos[1], dist = sqrtf(dx * dx + dy * dy);
    if (dist < 0.3f) return;
    const float err = MvWrap(atan2f(-dx, dy) - m->yaw), spot = 0.6f;
    const float close = MvClamp(dist / def->arrive, 0.25f, 1.0f);
    if (def->def.turn == HF_TURN_SPOT)
    {
        m->steer = MvClamp(err / spot, -1.0f, 1.0f);
        m->throttle = fabsf(err) > spot ? 0.0f : close * (1.0f - fabsf(err) / (spot * 2.0f));
        return;
    }
    m->steer = MvClamp(err / 0.5f, -1.0f, 1.0f);
    m->throttle = fabsf(err) > 1.7f ? 0.35f : close; /* behind it: a slow wide turn */
}

/* Speed and heading from the inputs, then the step over the ground. */
static void MvDrive(MvUnit* m, const HfMoverDef* d, float dt)
{
    const float want = m->throttle >= 0 ? m->throttle * d->speed : m->throttle * d->reverse_speed;
    const float dv = d->acceleration * dt;
    m->speed = want > m->speed + dv ? m->speed + dv : want < m->speed - dv ? m->speed - dv : want;
    float rate = d->turn_rate * 0.0174533f;
    if (d->turn == HF_TURN_VEHICLE) rate = fminf(rate, fabsf(m->speed) / d->turn_radius) * (m->speed < 0 ? -1.0f : 1.0f);
    m->turning = m->steer * rate;
    m->yaw = MvWrap(m->yaw + m->turning * dt);
    m->status = HF_MOVER_MOVING;
    if (fabsf(m->speed * dt) < 0.0005f) return;
    if (MvStep(m, d, m->speed * dt)) { m->blockedLogged = 0; return; }
    m->speed = 0;
    m->status = HF_MOVER_BLOCKED;
    if (m->blockedLogged++ == 0)
        ApiLog(g_mvDefs[m->def - 1].mod, "mover: unit %u blocked at %.1f %.1f %.1f (a step it can't take, or a wall)", m->entity,
            (double)m->pos[0], (double)m->pos[1], (double)m->pos[2]);
}

static void MvUnitTick(MvUnit* m, float dt)
{
    const MvDef* def = &g_mvDefs[m->def - 1];
    m->age += dt;
    m->inputAge += dt;
    uint32_t network = 0;
    if (!m->unit && !WcEntity(m->entity, &m->unit, &network)) return;
    if (m->age < def->def.start_delay) { m->status = HF_MOVER_WAITING; return; }
    if (!m->started)
    {
        if (!MvParent(m)) { m->status = HF_MOVER_WAITING; return; }
        m->started = 1;
        ApiLog(def->mod, "mover: unit %u moved by HellForge from %.1f %.1f %.1f", m->entity, (double)m->pos[0], (double)m->pos[1], (double)m->pos[2]);
    }
    if ((m->sinceSolid += dt) >= MV_SOLID_S) { m->sinceSolid = 0; MvListMobile(m); }
    if (m->inputAge < MV_INPUT_S) m->driver = HF_DRIVER_MOD;
    else if (def->ai) { m->driver = HF_DRIVER_AI; MvAiInputs(m, def); }
    else { m->driver = HF_DRIVER_NONE; m->throttle = 0; m->steer = 0; }
    MvDrive(m, &def->def, dt);
    MvSetRoot(m);
}

static void MvTick(HfMod* mod, float dt, void* user)
{
    (void)mod; (void)user;
    if (!g_mvPos || !g_mvRot) return;
    AcquireSRWLockExclusive(&g_mvLock);
    for (int i = 0; i < MV_UNITS; i++)
    {
        MvUnit* m = &g_mvUnits[i];
        if (!m->entity) continue;
        if (!EvpAlive(m->entity)) { memset(m, 0, sizeof *m); continue; }
        MvUnitTick(m, dt);
    }
    ReleaseSRWLockExclusive(&g_mvLock);
}

static void MvSpawned(HfMod* mod, const HfSpawnEvent* e, void* user)
{
    (void)mod; (void)user;
    if (!e->authority) return;
    for (LONG d = 0; d < g_mvDefCount; d++)
    {
        if (g_mvDefs[d].def.entity != e->resource) continue;
        AcquireSRWLockExclusive(&g_mvLock);
        for (int i = 0; i < MV_UNITS; i++)
            if (!g_mvUnits[i].entity) { memset(&g_mvUnits[i], 0, sizeof g_mvUnits[i]); g_mvUnits[i].entity = e->entity; g_mvUnits[i].def = (uint32_t)d + 1; break; }
        ReleaseSRWLockExclusive(&g_mvLock);
        return;
    }
}

static void MvStart(void)
{
    if (InterlockedExchange(&g_mvTicking, 1)) return;
    g_mvMod.api = &g_api;
    snprintf(g_mvMod.id, sizeof g_mvMod.id, "hellforge.movers");
    g_mvMod.log = g_log;
    g_mvMod.loaded = 1;
    g_mvPos = (MvPosFn)ApiFunction(&g_mvMod, "UnitTeleportLocalPosition");
    g_mvRot = (MvPosFn)ApiFunction(&g_mvMod, "UnitTeleportLocalRotation");
    g_mvLevel = (MvLevelFn)ApiFunction(&g_mvMod, "UnitLevel");
    if (!g_mvPos || !g_mvRot) Note("movers: off; the teleport calls are not the reviewed ones");
    ApiSubscribe(&g_mvMod, HF_ON_SPAWN, (HfListener)(void (*)(void))MvSpawned, NULL, 0);
    ApiOnGameThread(&g_mvMod, MvTick, NULL);
}

/* 1 with the mover's position when this machine moves the entity (ai_orders.c: units with no brain record). */
static int MvWhere(uint32_t entity, float out[3])
{
    if (!g_mvDefCount || !TryAcquireSRWLockShared(&g_mvLock)) return 0;
    const MvUnit* m = MvUnitOf(entity);
    int ok = m != NULL && m->started;
    if (ok) memcpy(out, m->pos, 12);
    ReleaseSRWLockShared(&g_mvLock);
    return ok;
}

/* 1 while the entity is a mover's unit whose root HellForge has not read yet (its start_delay). */
static int MvPending(uint32_t entity)
{
    if (!g_mvDefCount || !TryAcquireSRWLockShared(&g_mvLock)) return 0;
    const MvUnit* m = MvUnitOf(entity);
    int pending = m != NULL && !m->started;
    ReleaseSRWLockShared(&g_mvLock);
    return pending;
}

/* ---- the API ---- */

static void MvDefaults(HfMoverDef* d)
{
    if (!d->mode) d->mode = HF_MOVE_WALK;
    if (!d->turn) d->turn = HF_TURN_SPOT;
    if (d->speed == 0) d->speed = 4.0f;
    if (d->reverse_speed == 0) d->reverse_speed = d->speed * 0.5f;
    if (d->acceleration == 0) d->acceleration = 8.0f;
    if (d->turn_rate == 0) d->turn_rate = 120.0f;
    if (d->turn_radius == 0) d->turn_radius = 4.0f;
    if (d->step_up == 0) d->step_up = 0.5f;
    if (d->step_down == 0) d->step_down = 1.0f;
    if (d->reach == 0) d->reach = 0.6f;
}

static const char* MvRefusal(const HfMoverDef* in, HfMoverDef* d)
{
    if (in == NULL || in->size < sizeof *in || in->entity == 0) return "no HfMoverDef, its size or its entity";
    *d = *in;
    MvDefaults(d);
    if (d->mode > HF_MOVE_DRIVE || d->turn > HF_TURN_VEHICLE) return "mode WALK or DRIVE, turn SPOT or VEHICLE (hover and fly come later)";
    if (!AiIn(d->speed, 0.1f, 40) || !AiIn(d->reverse_speed, 0, 40) || !AiIn(d->acceleration, 0.1f, 200) || !AiIn(d->turn_rate, 1, 1440) ||
        !AiIn(d->turn_radius, 0.1f, 100) || !AiIn(d->step_up, 0, 10) || !AiIn(d->step_down, 0, 50) || !AiIn(d->reach, 0, 20) ||
        !AiIn(d->start_delay, 0, 120))
        return "speed 0.1 .. 40, reverse 0 .. 40, acceleration 0.1 .. 200, turn_rate 1 .. 1440, turn_radius 0.1 .. 100, steps 0 .. 10 up and 50 down, reach 0 .. 20, start_delay 0 .. 120";
    return NULL;
}

/* SDK 35, add_mover: HellForge moves the entity's units (hellforge_move.h). Any time, best from hellforge_mod_init.
 * 0 done, 1 refused (the log says why). */
static int ApiAddMover(HfMod* mod, const HfMoverDef* in)
{
    HfMoverDef d;
    const char* why = MvRefusal(in, &d);
    if (why) { ApiLog(mod, "add_mover: refused: %s", why); return 1; }
    for (LONG i = 0; i < g_mvDefCount; i++)
        if (g_mvDefs[i].def.entity == d.entity) { ApiLog(mod, "add_mover(%016llx): refused: it has a mover already", (unsigned long long)d.entity); return 1; }
    LONG at = InterlockedIncrement(&g_mvDefCount) - 1;
    if (at >= MV_DEFS) { InterlockedDecrement(&g_mvDefCount); ApiLog(mod, "add_mover: refused: %d movers already", MV_DEFS); return 1; }
    g_mvDefs[at] = (MvDef){mod, d, 0, 3.0f};
    ApiLog(mod, "add_mover(%016llx): %s, %.1f m/s (%.1f back), %s at %.0f deg/s, steps %.2f up %.2f down", (unsigned long long)d.entity,
        d.mode == HF_MOVE_DRIVE ? "drives" : "walks", (double)d.speed, (double)d.reverse_speed,
        d.turn == HF_TURN_VEHICLE ? "turns like a vehicle" : "turns on the spot", (double)d.turn_rate, (double)d.step_up, (double)d.step_down);
    MvStart();
    return 0;
}

/* SDK 35, mover_input: a driver's throttle and steer for the next half second. Game thread, on the host. 0 taken. */
static int ApiMoverInput(HfMod* mod, uint32_t entity, const HfMoverInput* in)
{
    if (!WcGameThread(mod, "mover_input") || in == NULL || in->size < sizeof *in) return 1;
    AcquireSRWLockExclusive(&g_mvLock);
    MvUnit* m = MvUnitOf(entity);
    if (m != NULL) { m->throttle = MvClamp(in->throttle, -1, 1); m->steer = MvClamp(in->steer, -1, 1); m->inputAge = 0; }
    ReleaseSRWLockExclusive(&g_mvLock);
    return m == NULL;
}

/* SDK 35, mover_state: how a mover's unit moves (HF_MOVER_...). Any thread. */
static uint32_t ApiMoverState(HfMod* mod, uint32_t entity, HfMoverState* state)
{
    (void)mod;
    HfMoverState s;
    memset(&s, 0, sizeof s);
    s.size = sizeof s;
    AcquireSRWLockShared(&g_mvLock);
    const MvUnit* m = MvUnitOf(entity);
    if (m != NULL)
    {
        s.status = m->started ? m->status : HF_MOVER_WAITING; s.driver = m->driver;
        memcpy(s.position, m->pos, 12);
        s.yaw = m->yaw; s.speed = m->speed; s.turning = m->turning; s.throttle = m->throttle; s.steer = m->steer;
    }
    ReleaseSRWLockShared(&g_mvLock);
    if (state != NULL && state->size >= sizeof s) *state = s;
    return s.status;
}

/* SDK 35, ai_drive: the custom AI drives the mover of `entity` (add_mover first). Any time. 0 done, 1 refused. */
static int ApiAiDrive(HfMod* mod, uint64_t entity, const HfAiDrive* in)
{
    if (in == NULL || in->size < sizeof *in) { ApiLog(mod, "ai_drive: refused: no HfAiDrive or its size"); return 1; }
    LONG d = 0;
    while (d < g_mvDefCount && g_mvDefs[d].def.entity != entity) d++;
    if (d == g_mvDefCount) { ApiLog(mod, "ai_drive(%016llx): refused: add_mover it first", (unsigned long long)entity); return 1; }
    HfAiObey obey;
    memset(&obey, 0, sizeof obey);
    obey.size = sizeof obey; obey.entity = entity; obey.range = in->range; obey.follow_distance = in->follow_distance;
    obey.hold_radius = in->hold_radius; obey.start_order = in->start_order; obey.nav_preset = in->nav_preset;
    if (ApiAiObey(mod, &obey)) return 1;
    g_mvDefs[d].arrive = in->arrive_distance > 0 ? in->arrive_distance : 3.0f;
    g_mvDefs[d].ai = 1;
    ApiLog(mod, "ai_drive(%016llx): the custom AI drives it (slows within %.1f m)", (unsigned long long)entity, (double)g_mvDefs[d].arrive);
    return 0;
}

/* The table's entries from SDK 35 on (ApiInit). */
#define MV_API ApiAddMover, ApiMoverInput, ApiMoverState, ApiAiDrive
