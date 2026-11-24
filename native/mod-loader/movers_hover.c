/* SDK 36: hover movers (HF_MOVE_HOVER, docs/modding/movers.md "Hover"). A hover vehicle stays the game's physics body, so
 * the engine collides and replicates it and its seats work; HellForge only pushes its chassis, where it is simulated:
 * heave toward hover.height over the ground or water (a spring-damper on the weight its data does not carry), righting
 * roll and pitch, thrust and brake along its level heading, air and sideways drag, and a yaw rate from the steer.
 * The calls are mappings/functions.json's Actor* rows. Included by movers.c after MvDriver. */

#define HV_ENGAGE 2.0f      /* hover heights: it takes hold once the surface is this close (never under the Pelican, ~10 m) */
#define HV_REACH 30.0f      /* metres: the ray down, and the gap beyond which a reading is no good */
#define HV_LOST_S 30.0f     /* no good reading this long and it lets go; until then it holds over the last surface it saw */
#define HV_DROP_MAX 4.0f    /* m/s: the fastest it lets itself sink toward a surface far below (off a ledge) */
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
    Note(g_hvState <= 0 ? "movers: hover off; the physics calls are not the reviewed ones" : g_mvWater ? "movers: hover on, over water too"
        : "movers: hover on, but blind to water (WaterHeight is not the reviewed code)");
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
static float HvSurface(MvUnit* m, const float p[3], float reach)
{
    HfSight how = {sizeof how, {m->unit, 0, 0, 0}, HV_SKIP, 0};
    const float to[3] = {p[0], p[1], p[2] - reach};
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
    const int wet = w > -499.0f && w < p[2] + 3.0f && w > ground; /* a craft sunk under the surface still floats up */
    m->waterDepth = wet ? (ground > -1e8f ? w - ground : reach) : 0;
    return wet ? w : ground;
}

/* The horizontal push it wants, m/s2: thrust or brake along its level heading, or a mod's push; drag on top. */
static void HvDrive(MvUnit* m, const MvDef* def, const float fl[2], const float v[3], float dt, float a[2], float* yawRate)
{
    const HfMoverDef* d = &def->def;
    const float vf = v[0] * fl[0] + v[1] * fl[1], side = -fl[1] * v[0] + fl[0] * v[1], vh = sqrtf(v[0] * v[0] + v[1] * v[1]);
    float push = 0, sidePush = 0;
    a[0] = a[1] = 0;
    if (d->drive != NULL)
    {
        const HfMoverPush p = MvPush(m, def, dt);
        push = p.forward; sidePush = p.side;
        *yawRate = p.turn;
    }
    else
    {
        const float t = m->throttle, top = MvTop(m, d, dt);
        *yawRate = m->steer * MvRate(d, vf);
        sidePush = m->strafe * d->hover.side_drag * d->strafe_speed; /* the side drag tops a full strafe out at strafe_speed */
        if ((t * vf < 0 && fabsf(vf) > 0.3f) || (fabsf(t) < 0.05f && fabsf(m->strafe) < 0.05f && vh < 1.5f) || (m->lost > 0 && !MvFlier(d->mode)))
        {   /* braking (or holding still): against the whole horizontal speed, no further than to a stop */
            const float b = vh > 1e-3f ? fminf(d->brake, vh / dt) / vh : 0;
            a[0] = -b * v[0]; a[1] = -b * v[1];
        }
        else if (t > 0) push = t * d->acceleration * (top * top) / (d->speed * d->speed); /* a boost: the fans push harder */
        else push = t * d->acceleration * (d->reverse_speed * d->reverse_speed) / (d->speed * d->speed); /* top speed backward is reverse_speed */
    }
    const float cd = d->acceleration / (d->speed * d->speed); /* quadratic air drag: full thrust tops out at speed */
    a[0] += push * fl[0] - sidePush * fl[1] - cd * vh * v[0] + d->hover.side_drag * side * fl[1];
    a[1] += push * fl[1] + sidePush * fl[0] - cd * vh * v[1] - d->hover.side_drag * side * fl[0];
}

/* Its gap over the surface, and whether it hovers: it takes hold once the surface is close, lets go when it is lost. */
static float HvGap(MvUnit* m, const MvDef* def, const float p[3], float dt)
{
    const float surface = HvSurface(m, p, HV_REACH);
    const int good = surface > -1e8f && p[2] - surface > -3.0f && p[2] - surface < HV_REACH;
    if (good) m->lastSurface = surface;
    /* deep water it can't read, or a drop: it holds over the last surface it saw rather than falling in */
    const float gap = p[2] - (good || !m->engaged ? surface : m->lastSurface);
    m->height = good ? gap : 0;
    m->lost = good ? 0 : m->lost + dt;
    if (!m->engaged && good && gap < HV_ENGAGE * def->def.hover.height)
    {
        m->targetHeight = def->def.hover.height;
        m->engaged = 1;
        ApiLog(def->mod, "mover: unit %u: %s %.2f m under it: it hovers", m->entity, m->waterDepth > 0 ? "water" : "ground", (double)gap);
    }
    m->targetHeight = MvClamp(m->targetHeight + m->lift * def->def.hover.climb_rate * dt, def->def.hover.height_min, def->def.hover.height_max);
    if (m->engaged && m->lost > 0 && m->lost <= dt) ApiLog(def->mod, "mover: unit %u: no surface in reach: it holds its height and stops", m->entity);
    if (m->engaged && m->lost > HV_LOST_S) { m->engaged = 0; ApiLog(def->mod, "mover: unit %u: no surface in reach for %.0f s: it lets go", m->entity, (double)HV_LOST_S); }
    return gap;
}

/* The ground's height under (x, y), or `fallback`. */
static float HvGroundAt(const MvUnit* m, float x, float y, float z, float fallback)
{
    HfSight how = {sizeof how, {m->unit, 0, 0, 0}, HV_SKIP, 0};
    const float from[3] = {x, y, z}, to[3] = {x, y, z - HV_REACH};
    HfSightHit hit;
    memset(&hit, 0, sizeof hit);
    return ApiLineOfSight(&g_mvMod, from, to, &how, &hit) == HF_SIGHT_BLOCKED && hit.unit == 0 && isfinite(hit.position[2]) ? hit.position[2] : fallback;
}

/* The up it rights itself to: the world's, or with a tilt_limit the ground's under it (water is level), at most
 * tilt_limit from the world's. */
static void HvTargetUp(const MvUnit* m, const HfMoverDef* d, const float* w, float n[3])
{
    n[0] = n[1] = 0; n[2] = 1;
    if (d->tilt_limit <= 0 || m->waterDepth > 0) return;
    const float* r = w, * f = w + 4, * p = w + 12, here = p[2] - m->height;
    const float zf = HvGroundAt(m, p[0] + f[0] * 1.5f, p[1] + f[1] * 1.5f, p[2], here), zr = HvGroundAt(m, p[0] + r[0], p[1] + r[1], p[2], here);
    const float sf = (zf - here) / 1.5f, sr = zr - here; /* rise per metre ahead and to the right */
    const float lim = tanf(d->tilt_limit * 0.0174533f), gf = MvClamp(sf, -lim, lim), gr = MvClamp(sr, -lim, lim);
    /* the normal of a plane rising gf along f and gr along r (both level-projected) */
    n[0] = -(gf * f[0] + gr * r[0]); n[1] = -(gf * f[1] + gr * r[1]);
    const float l = sqrtf(n[0] * n[0] + n[1] * n[1] + 1);
    n[0] /= l; n[1] /= l; n[2] = 1 / l;
}

/* Righting toward `n` about roll and pitch (up x n), harder past tilt_limit; yaw toward the wanted rate. Inertia scales
 * as an FRV's. */
static void HvAttitude(MvUnit* m, const HfMoverDef* d, const float* w, const float om[3], const float n[3], float yawRate, float dt)
{
    const float* up = w + 8;
    const float wn = 6.2831853f * d->hover.stiffness, zeta = fminf(d->hover.damping + 0.1f, 2.0f);
    const float past = up[2] < cosf((d->tilt_limit > 0 ? d->tilt_limit : 30.0f) * 0.0174533f) ? 3.0f : 1.0f, k = 0.9f * d->hover.mass * wn * wn * past;
    const float ex = up[1] * n[2] - up[2] * n[1], ey = up[2] * n[0] - up[0] * n[2];
    const float c = 0.9f * d->hover.mass * 2 * zeta * wn;
    const float t[3] = {(k * ex - c * om[0]) * dt, (k * ey - c * om[1]) * dt, d->hover.mass * (yawRate - om[2]) / 0.4f * dt};
    g_hvAngular(m->body, t);
    m->pitch = asinf(MvClamp(w[6], -1, 1));
    m->roll = asinf(MvClamp(-w[2], -1, 1));
}

/* Its root's world pose (w), its body's speeds, and m's position and heading from them: 0 when the pose can't be read. */
static int HvRead(MvUnit* m, float w[16], float v[3], float om[3])
{
    if (!ApiJointPose(&g_mvMod, m->unit, 0, HF_POSE_WORLD, w) || !isfinite(w[12])) return 0;
    g_hvVelocity(m->body, v, om);
    memcpy(m->pos, w + 12, 12);
    m->yaw = atan2f(-w[4], w[5]);
    return 1;
}

/* One update of a hover on the machine that simulates it. */
static void HvControl(MvUnit* m, const MvDef* def, float dt)
{
    const HfMoverDef* d = &def->def;
    float w[16], v[3] = {0}, om[3] = {0};
    if (!HvRead(m, w, v, om)) return;
    const float* f = w + 4, * p = w + 12;
    const float gap = HvGap(m, def, p, dt), mass = d->hover.mass;
    if (!m->engaged)
    {
        m->status = HF_MOVER_WAITING;
        if (v[2] > 0.5f) { const float j[3] = {0, 0, -0.3f * MV_G * mass * dt}; g_hvImpulse(m->body, j, NULL); } /* no drifting up */
        return;
    }
    m->status = HF_MOVER_MOVING;
    /* heave: the weight its data leaves, plus a spring-damper toward its height (hover.height, moved by lift) */
    const float wn = 6.2831853f * d->hover.stiffness;
    const float low = gap > m->targetHeight + 1.0f ? -0.15f * MV_G : -0.55f * MV_G; /* far above: it sinks, it doesn't dive */
    float lift = MvClamp(wn * wn * (m->targetHeight - gap) - 2 * d->hover.damping * wn * v[2], low, 0.95f * MV_G);
    if (v[2] > HV_CLIMB_MAX) lift = fminf(lift, fmaxf(-2.0f * (v[2] - HV_CLIMB_MAX), low));
    if (v[2] < -HV_DROP_MAX) lift = fmaxf(lift, fminf(2.0f * (-HV_DROP_MAX - v[2]), 0.95f * MV_G));
    const float az = (1.0f - d->hover.carried) * MV_G + lift;
    float fl[2] = {f[0], f[1]}, a[2] = {0, 0}, yawRate = 0;
    const float len = sqrtf(fl[0] * fl[0] + fl[1] * fl[1]);
    if (len > 1e-3f) { fl[0] /= len; fl[1] /= len; HvDrive(m, def, fl, v, dt, a, &yawRate); }
    const float j[3] = {a[0] * mass * dt, a[1] * mass * dt, az * mass * dt};
    g_hvImpulse(m->body, j, NULL);
    float n[3];
    HvTargetUp(m, d, w, n);
    HvAttitude(m, d, w, om, n, yawRate, dt);
    m->speed = len > 1e-3f ? v[0] * fl[0] + v[1] * fl[1] : 0;
    m->turning = om[2];
}

#include "movers_fly.c" /* SDK 43: helicopters and planes (HvSurface, HvDrive, HvTargetUp, HvAttitude) */
#include "movers_boat.c" /* SDK 43: boats and submarines (HvGroundAt, HvDrive, HvAttitude) */

/* Every 2 s for a minute from its start and from each change of driver, and while it is lost or over water. */
static void HvLog(MvUnit* m, const MvDef* def, float dt)
{
    if (m->logUntil <= 0 || m->driver != m->loggedDriver) { m->logUntil = (m->logUntil <= 0 ? def->def.start_delay : m->age) + 60; m->loggedDriver = m->driver; m->sinceLog = 2.0; }
    if ((m->sinceLog += dt) < 2.0 || (m->age > m->logUntil && m->lost <= 0 && m->waterDepth <= 0)) return;
    m->sinceLog = 0;
    ApiLog(def->mod, "mover: unit %u %s %s, %.2f m over %s (target %.2f, water query %.2f), speed %.1f, throttle %.2f, steer %.2f, driver %u", m->entity, MvFlier(def->def.mode) ? "flight" : MvVessel(def->def.mode) ? "sail" : "hover",
        m->engaged ? "on" : "off", (double)m->height, m->waterDepth > 0 ? "water" : "ground", (double)m->targetHeight, (double)MvWaterAt(m->pos[0], m->pos[1], m->pos[2]),
        (double)m->speed, (double)m->throttle, (double)m->steer, m->driver);
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
    if (MvFlier(def->def.mode)) FlControl(m, def, dt);
    else if (MvVessel(def->def.mode)) BtControl(m, def, dt);
    else HvControl(m, def, dt);
    MvSounds(m, &def->def.sounds);
    HvLog(m, def, dt);
}

/* The seated drivers' inputs, read where the game reads them (VehicleMotionManager +0x60 steer, throttle, brake; the
 * drive state's +0x618 lever: 0 reverse, 1 neutral). The game's steer is positive to the right. Under g_mvLock. */
#define HV_LIFT_HOLD_S 30.0 /* the lift keys count for a craft whose seat drove it this recently: its driver is here */

/* Space up, Control down (Jan's keys), while the game's window has the keyboard. */
static float HvLiftKeys(void)
{
    DWORD process = 0;
    const HWND w = GetForegroundWindow();
    if (w == NULL || !GetWindowThreadProcessId(w, &process) || process != GetCurrentProcessId()) return 0;
    return ((GetAsyncKeyState(VK_SPACE) & 0x8000) ? 1.0f : 0.0f) - ((GetAsyncKeyState(VK_CONTROL) & 0x8000) ? 1.0f : 0.0f);
}

/* One vehicle's record, input and lever. */
static void HvSeat(uintptr_t record, uintptr_t input, uintptr_t leverAt)
{
    uintptr_t r;
    uint32_t entity, lever;
    float in[3];
    if (!EvpPtr(record, &r) || SafeRead(r + 8, &entity, 4)) return;
    MvUnit* m = MvUnitOf(entity);
    if (m == NULL || m->remote || SafeRead(input, in, 12) || SafeRead(leverAt, &lever, 4) ||
        !AiIn(in[0], -1.01f, 1.01f) || !AiIn(in[1], -0.01f, 1.01f) || !AiIn(in[2], -0.01f, 1.01f))
        return;
    const float throttle = lever == 0 ? -fmaxf(in[1], in[2]) : lever == 1 ? 0.0f : in[1] - in[2];
    m->seatThrottle = MvClamp(throttle, -1, 1);
    m->seatSteer = MvClamp(-in[0], -1, 1);
    m->seatLift = m->seatAge < HV_LIFT_HOLD_S ? HvLiftKeys() : 0; /* on foot, Space is a jump: only a craft just driven here */
    if (fabsf(throttle) > 0.02f || fabsf(in[0]) > 0.02f || m->seatLift != 0) m->seatAge = 0;
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

/* The first mover hooks the vehicles' motion update for their seats (chained: mods may hook it too). */
static void HvStart(void)
{
    if (InterlockedExchange(&g_hvHooked, 1)) return;
    if (ApiHookFunction(&g_mvMod, "VehicleMotionUpdate", (void*)HvMotionDetour, (void**)&g_hvMotionNext, 0))
        Note("movers: vehicles can't be driven from their seats (VehicleMotionUpdate not hooked)");
}
