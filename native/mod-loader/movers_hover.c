/* SDK 36: hover movers (HF_MOVE_HOVER, docs/modding/movers.md "Hover"). A hover vehicle stays the game's physics body, so
 * the engine collides and replicates it and its seats work; HellForge only pushes its chassis, where it is simulated:
 * heave toward hover_height over the ground or water (a spring-damper on the weight its data does not carry), righting
 * roll and pitch, thrust and brake along its level heading, air and sideways drag, and a yaw rate from the steer.
 * The calls are mappings/functions.json's Actor* rows. Included by movers.c after MvDriver. */

#define HV_ENGAGE 2.0f      /* hover heights: it takes hold once the surface is this close (never under the Pelican, ~10 m) */
#define HV_REACH 30.0f      /* metres: the ray down, and the gap beyond which a reading is no good */
#define HV_LOST_S 0.5f      /* no good reading this long and it lets go: it settles on the share its data carries */
#define HV_CLIMB_MAX 2.0f   /* m/s: it never pushes up while climbing faster (a seated crew once read as ground) */
#define HV_SKIP 0.5f        /* metres: ray hits nearer its root are its own parts or crew */
#define HV_EXE_ACTORS 0x27CD910u /* helldivers2.exe: the actor table, whose +0xC0 must hold ActorAddImpulse */
#define HV_EXE_WORLDS 0x27BA8A8u /* the physics worlds, 0xB0 apart: a world's vtable +0x70 gives a body by its id */

typedef void (*HvImpulseFn)(uint32_t actor, const float* impulse, const float* point);
typedef void (*HvAngularFn)(uint32_t actor, const float* impulse);
typedef void (*HvVelocityFn)(uint32_t actor, float* linear, float* angular);
typedef uint64_t (*HvActorsFn)(uint32_t unit, uint32_t mask, uint64_t* out);
typedef uint8_t (*HvResolvesFn)(uint32_t actor);
typedef uint32_t (*HvUnitFn)(uint32_t actor);
typedef const uint8_t* (*HvRecordFn)(uint32_t actor);
typedef const uint8_t* (*HvBodyFn)(void* world, uint32_t body);
typedef void (*HvMotionFn)(void* manager, float dt);

static HvImpulseFn g_hvImpulse;
static HvAngularFn g_hvAngular;
static HvVelocityFn g_hvVelocity;
static HvActorsFn g_hvActors;
static HvResolvesFn g_hvResolves;
static HvUnitFn g_hvUnit;
static HvRecordFn g_hvRecord;
static HvMotionFn g_hvMotionNext;
static int g_hvState; /* 0 not asked yet, 1 ready, -1 off */
static volatile LONG g_hvHooked;

/* Every physics call, once the game is loaded: 1 when all read as reviewed and the actor table holds the impulse. */
static int HvReady(void)
{
    if (g_hvState) return g_hvState > 0;
    const uintptr_t exe = (uintptr_t)GetModuleHandleW(NULL);
    g_hvImpulse = (HvImpulseFn)ApiFunction(&g_mvMod, "ActorAddImpulse");
    g_hvAngular = (HvAngularFn)ApiFunction(&g_mvMod, "ActorAddAngularImpulse");
    g_hvVelocity = (HvVelocityFn)ApiFunction(&g_mvMod, "ActorVelocity");
    g_hvActors = (HvActorsFn)ApiFunction(&g_mvMod, "UnitActors");
    g_hvResolves = (HvResolvesFn)ApiFunction(&g_mvMod, "ActorResolves");
    g_hvUnit = (HvUnitFn)ApiFunction(&g_mvMod, "ActorUnit");
    g_hvRecord = (HvRecordFn)ApiFunction(&g_mvMod, "ActorRecord");
    uintptr_t slot = 0;
    const int table = EvpPtr(exe + HV_EXE_ACTORS + 0xC0, &slot) && slot == (uintptr_t)g_hvImpulse;
    g_hvState = g_hvImpulse && g_hvAngular && g_hvVelocity && g_hvActors && g_hvResolves && g_hvUnit && g_hvRecord && table ? 1 : -1;
    Note(g_hvState > 0 ? "movers: hover on" : "movers: hover off; the physics calls are not the reviewed ones");
    return g_hvState > 0;
}

/* The hknp body flags of an actor's body, or -1. */
static int64_t HvBodyFlags(uint32_t actor)
{
    const uintptr_t exe = (uintptr_t)GetModuleHandleW(NULL);
    const uint8_t* rec = g_hvRecord(actor);
    uintptr_t world, vtable, fn;
    uint32_t id, flags;
    if (rec == NULL || SafeRead((uintptr_t)rec + 0x14, &id, 4) || !EvpPtr(exe + HV_EXE_WORLDS + 0xB0u * (actor >> 30), &world) ||
        !EvpPtr(world, &vtable) || !EvpPtr(vtable + 0x70, &fn) || fn < exe)
        return -1;
    const uint8_t* body = ((HvBodyFn)fn)((void*)world, id);
    return body == NULL || SafeRead((uintptr_t)body + 0x44, &flags, 4) ? -1 : (int64_t)flags;
}

/* The chassis: the unit's own actor whose body takes impulses (hknp flags bit 1). 0 when it has none. */
static uint32_t HvChassis(const MvUnit* m)
{
    uint64_t out[12] = {0};
    uint32_t count = 0;
    if (g_hvActors(m->unit, 1, out) == 0xFFFFFFFFull || !out[0]) return 0;
    memcpy(&count, (const uint8_t*)out + 0x38, 4);
    for (uint32_t k = 0; k < count && k < 64; k++)
    {
        uint32_t a;
        if (SafeRead((uintptr_t)out[0] + 4u * k, &a, 4) || a == 0xFFFFFFFFu || !g_hvResolves(a) || g_hvUnit(a) != m->unit) continue;
        const int64_t f = HvBodyFlags(a);
        if (f >= 0 && (f & 2)) return a;
    }
    return 0;
}

/* The surface under p: the ground (rays pass units, its crew among them) or the water over it, the higher. -1e9 none. */
static float HvSurface(MvUnit* m, const float p[3])
{
    HfSight how = {sizeof how, {m->unit, 0, 0, 0}, HV_SKIP, 0};
    const float to[3] = {p[0], p[1], p[2] - HV_REACH};
    float ground = -1e9f;
    for (int k = 1; k <= 4; k++)
    {
        HfSightHit hit;
        memset(&hit, 0, sizeof hit);
        if (ApiLineOfSight(&g_mvMod, p, to, &how, &hit) != HF_SIGHT_BLOCKED || !isfinite(hit.position[2])) break;
        if (hit.unit == 0) { ground = hit.position[2]; break; }
        if (k < 4) how.ignore[k] = hit.unit;
    }
    const float w = MvWaterAt(p[0], p[1], p[2]);
    const int wet = w > -499.0f && w < p[2] && w > ground;
    m->waterDepth = wet ? (ground > -1e8f ? w - ground : HV_REACH) : 0;
    return wet ? w : ground;
}

/* The horizontal push it wants, m/s2: thrust or brake along its level heading, or a mod's push; drag on top. */
static void HvDrive(MvUnit* m, const MvDef* def, const float fl[2], const float v[3], float dt, float a[2], float* yawRate)
{
    const HfMoverDef* d = &def->def;
    const float vf = v[0] * fl[0] + v[1] * fl[1], side = -fl[1] * v[0] + fl[0] * v[1], vh = sqrtf(v[0] * v[0] + v[1] * v[1]);
    float push = 0, sidePush = 0;
    a[0] = a[1] = 0;
    if (d->drivetrain == HF_DRIVETRAIN_NONE)
    {
        const HfMoverPush p = MvPush(m, def, dt);
        push = p.forward; sidePush = p.side;
        *yawRate = p.turn;
    }
    else
    {
        const float t = m->throttle;
        *yawRate = m->steer * MvRate(d, vf);
        if ((t * vf < 0 && fabsf(vf) > 0.3f) || (fabsf(t) < 0.05f && vh < 1.5f))
        {   /* braking (or holding still): against the whole horizontal speed, no further than to a stop */
            const float b = vh > 1e-3f ? fminf(d->brake, vh / dt) / vh : 0;
            a[0] = -b * v[0]; a[1] = -b * v[1];
        }
        else if (t > 0) push = t * d->acceleration;
        else push = t * d->acceleration * (d->reverse_speed * d->reverse_speed) / (d->speed * d->speed); /* top speed backward is reverse_speed */
    }
    const float cd = d->acceleration / (d->speed * d->speed); /* quadratic air drag: full thrust tops out at speed */
    a[0] += push * fl[0] - sidePush * fl[1] - cd * vh * v[0] + d->side_drag * side * fl[1];
    a[1] += push * fl[1] + sidePush * fl[0] - cd * vh * v[1] - d->side_drag * side * fl[0];
}

/* Its gap over the surface, and whether it hovers: it takes hold once the surface is close, lets go when it is lost. */
static float HvGap(MvUnit* m, const MvDef* def, const float p[3], float dt)
{
    const float surface = HvSurface(m, p), gap = p[2] - surface;
    const int good = surface > -1e8f && gap > 0 && gap < HV_REACH;
    m->height = good ? gap : 0;
    m->lost = good ? 0 : m->lost + dt;
    if (!m->engaged && good && gap < HV_ENGAGE * def->def.hover_height)
    {
        m->engaged = 1;
        ApiLog(def->mod, "mover: unit %u: %s %.2f m under it: it hovers", m->entity, m->waterDepth > 0 ? "water" : "ground", (double)gap);
    }
    if (m->engaged && m->lost > HV_LOST_S) { m->engaged = 0; ApiLog(def->mod, "mover: unit %u: no surface in reach: it lets go", m->entity); }
    return gap;
}

/* One update of a hover on the machine that simulates it. */
static void HvControl(MvUnit* m, const MvDef* def, float dt)
{
    const HfMoverDef* d = &def->def;
    float w[16], v[3] = {0}, om[3] = {0};
    if (!ApiJointPose(&g_mvMod, m->unit, 0, HF_POSE_WORLD, w) || !isfinite(w[12])) return;
    const float* f = w + 4, * up = w + 8, * p = w + 12;
    g_hvVelocity(m->body, v, om);
    memcpy(m->pos, p, 12);
    m->yaw = atan2f(-f[0], f[1]);
    const float gap = HvGap(m, def, p, dt), mass = d->mass;
    if (!m->engaged)
    {
        m->status = HF_MOVER_WAITING;
        if (v[2] > 0.5f) { const float j[3] = {0, 0, -0.3f * MV_G * mass * dt}; g_hvImpulse(m->body, j, NULL); } /* no drifting up */
        return;
    }
    m->status = HF_MOVER_MOVING;
    /* heave: the weight its data leaves, plus a spring-damper toward hover_height */
    const float wn = 6.2831853f * d->hover_hz;
    float lift = MvClamp(wn * wn * (d->hover_height - gap) - 2 * d->hover_damping * wn * v[2], -0.55f * MV_G, 0.95f * MV_G);
    if (v[2] > HV_CLIMB_MAX) lift = fminf(lift, fmaxf(-2.0f * (v[2] - HV_CLIMB_MAX), -0.55f * MV_G));
    const float az = (1.0f - d->carried) * MV_G + lift;
    float fl[2] = {f[0], f[1]}, a[2] = {0, 0}, yawRate = 0;
    const float len = sqrtf(fl[0] * fl[0] + fl[1] * fl[1]);
    if (len > 1e-3f) { fl[0] /= len; fl[1] /= len; HvDrive(m, def, fl, v, dt, a, &yawRate); }
    const float j[3] = {a[0] * mass * dt, a[1] * mass * dt, az * mass * dt};
    g_hvImpulse(m->body, j, NULL);
    /* attitude: roll and pitch back to level (up x world up), yaw toward the wanted rate. Inertia scales as an FRV's. */
    const float tilt = 0.9f * mass, yawI = mass, zeta = fminf(d->hover_damping + 0.1f, 2.0f);
    const float t[3] = {tilt * (wn * wn * up[1] - 2 * zeta * wn * om[0]) * dt, tilt * (wn * wn * -up[0] - 2 * zeta * wn * om[1]) * dt,
                        yawI * (yawRate - om[2]) / 0.4f * dt};
    g_hvAngular(m->body, t);
    m->speed = len > 1e-3f ? v[0] * fl[0] + v[1] * fl[1] : 0;
    m->turning = om[2];
}

static void HvUnitTick(MvUnit* m, const MvDef* def, float dt)
{
    if (!HvReady()) { m->status = HF_MOVER_WAITING; return; }
    if (ApiIsAuthority(NULL, m->entity) != 1) { m->status = HF_MOVER_REMOTE; m->engaged = 0; return; }
    if (!m->body || !g_hvResolves(m->body) || g_hvUnit(m->body) != m->unit)
    {
        m->body = HvChassis(m);
        if (!m->body)
        {
            if (!m->told++) ApiLog(def->mod, "mover: unit %u has no physics body that takes pushes: it can't hover", m->entity);
            m->status = HF_MOVER_WAITING; m->started = 0;
            return;
        }
        if (!m->started) ApiLog(def->mod, "mover: unit %u hovers by HellForge (chassis actor %08x)", m->entity, m->body);
        m->started = 1;
    }
    MvDriver(m, def);
    HvControl(m, def, dt);
    if ((m->sinceLog += dt) < 2.0 || m->age > def->def.start_delay + 60) return;
    m->sinceLog = 0;
    ApiLog(def->mod, "mover: unit %u hover %s, %.2f m over %s, speed %.1f, throttle %.2f, steer %.2f, driver %u", m->entity, m->engaged ? "on" : "off",
        (double)m->height, m->waterDepth > 0 ? "water" : "ground", (double)m->speed, (double)m->throttle, (double)m->steer, m->driver);
}

/* The seated drivers' inputs, read where the game reads them (VehicleMotionManager +0x60 steer, throttle, brake; the
 * drive state's +0x618 lever: 0 reverse, 1 neutral). The game's steer is positive to the right. Under g_mvLock. */
/* One vehicle's record, input and lever. */
static void HvSeat(uintptr_t record, uintptr_t input, uintptr_t leverAt)
{
    uintptr_t r;
    uint32_t entity, lever;
    float in[3];
    if (!EvpPtr(record, &r) || SafeRead(r + 8, &entity, 4)) return;
    MvUnit* m = MvUnitOf(entity);
    if (m == NULL || g_mvDefs[m->def - 1].def.mode != HF_MOVE_HOVER || SafeRead(input, in, 12) || SafeRead(leverAt, &lever, 4) ||
        !AiIn(in[0], -1.01f, 1.01f) || !AiIn(in[1], -0.01f, 1.01f) || !AiIn(in[2], -0.01f, 1.01f))
        return;
    const float throttle = lever == 0 ? -fmaxf(in[1], in[2]) : lever == 1 ? 0.0f : in[1] - in[2];
    m->seatThrottle = MvClamp(throttle, -1, 1);
    m->seatSteer = MvClamp(-in[0], -1, 1);
    if (fabsf(throttle) > 0.02f || fabsf(in[0]) > 0.02f) m->seatAge = 0;
}

static void HvSeats(uintptr_t manager)
{
    uint32_t count;
    uintptr_t records, inputs, drive;
    if (SafeRead(manager + 0x34, &count, 4) || !EvpPtr(manager + 0x58, &records) || !EvpPtr(manager + 0x60, &inputs) || !EvpPtr(manager + 0x70, &drive))
        return;
    for (uint32_t i = 0; i < count && i < 4096; i++) HvSeat(records + 8u * i, inputs + 0x10u * i, drive + 0x670u * i + 0x618);
}

static void HvMotionDetour(void* manager, float dt)
{
    if (manager != NULL && TryAcquireSRWLockExclusive(&g_mvLock))
    {
        HvSeats((uintptr_t)manager);
        ReleaseSRWLockExclusive(&g_mvLock);
    }
    g_hvMotionNext(manager, dt);
}

/* The first hover mover hooks the vehicles' motion update for its seats (chained: mods may hook it too). */
static void HvStart(void)
{
    if (InterlockedExchange(&g_hvHooked, 1)) return;
    if (ApiHookFunction(&g_mvMod, "VehicleMotionUpdate", (void*)HvMotionDetour, (void**)&g_hvMotionNext, 0))
        Note("movers: hover vehicles can't be driven from their seats (VehicleMotionUpdate not hooked)");
}
