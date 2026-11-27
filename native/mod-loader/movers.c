/* SDK 35: movers (docs/modding/movers.md, hellforge_move.h). A walker's or driver's unit is moved by HellForge on the
 * host: its root node is teleported each update, inside the frame of the unit it is parented to (a hellpod), from a
 * throttle and a steer. The ground is found with rays (line_of_sight); movers_sync.c sends its pose to the other machines.
 * SDK 36's hover vehicles stay physics bodies, pushed by movers_hover.c where they are simulated.
 * Included by world_calls.c after ai_orders.c (AiDrivePoint, ApiAiObey). */

#define MV_DEFS 32
#define MV_UNITS 64
#define MV_KIT_SLOTS 8   /* fixed guns on one mover */
#define MV_GUN_SEATS (1 + HF_GUN_FALLBACK_SEATS) /* a fixed gun's seat and its fallbacks (SDK 53) */
#define MV_ZONE_SLOTS 24 /* damage zones on one mover: armour plates and rotors need more than guns do */
#define MV_INPUT_S 0.5     /* a mod's input holds this long */
#define MV_SOLID_S 0.5     /* how often the moving units near a mover are listed */
#define MV_G 9.81f
#define MV_SLOPE_BASE 0.5f /* metres: an incline is read over at least this, so a millimetre step reads no slope */
#define MV_SLOPE_RUN 2.0f  /* metres: the incline is averaged over this much travel; a shorter rise is a step */
#define MV_WATER_STATE 0x347CE48u /* game.dll: the water state WaterHeight takes (its +0x44 byte: the map has water) */
#define MV_GAME_LEVEL 0xAE288u    /* the Game's mission level, WaterHeight's third argument (mappings/globals.json) */

typedef void (*MvPosFn)(uint32_t unit, int32_t node, const float* position);
typedef void* (*MvLevelFn)(uint32_t unit);
typedef float (*MvWaterFn)(void* state, const float* position, void* level);

typedef struct MvDef { HfMod* mod; HfMoverDef def; int ai; float arrive, slopeTan; uint32_t axles, asked; } MvDef; /* axles: HF_AXLE_ bits; asked: its mode */
/* A hover, flier or vessel: a physics body HellForge pushes (movers_hover.c, movers_fly.c), followed on every machine. */
static int MvFlier(uint32_t mode) { return mode == HF_MOVE_HELI || mode == HF_MOVE_PLANE; }
static int MvVessel(uint32_t mode) { return mode == HF_MOVE_BOAT || mode == HF_MOVE_SUB; }
static int MvPhysical(uint32_t mode) { return mode == HF_MOVE_HOVER || MvFlier(mode) || MvVessel(mode); }

/* A mover's pose as the machine that moves it sends it to the others (movers_sync.c). */
typedef struct MvWire
{
    uint64_t resource;
    uint32_t entity, network; /* the sender's entity (its key) and network id (WC_NO_NETWORK: none) */
    uint8_t status, driver, driven, boosting;
    float pos[3], start[3], yaw, pitch, roll, speed, turning, grade, pad;
} MvWire;

typedef struct MvUnit
{
    uint32_t entity, unit, def, started, status, driver;
    float pos[3], yaw, speed, turning, throttle, steer, par[4][3], parYaw, put[4];
    uint32_t written, reframed, undone, physics; /* physics: driven through its body, which undid HellForge's moves */
    double age, inputAge, sinceSolid, sinceTrace;
    uint32_t mobile[32], mobileCount, blockedLogged;
    float height, waterDepth, grade, climb, pitch, roll, side, strafe, lift, boost, boostLeft, targetHeight;
    uint32_t driven, boosting, wasMoving, wasBlocked, sounded;
    /* hover (movers_hover.c) */
    uint32_t body, engaged, told;
    float lost, lastSurface, seatThrottle, seatSteer, seatLift;
    float flWant, flIdle; /* a flier: the height it climbs toward, and how long nobody has flown it (movers_fly.c) */
    float flDatum;        /* a flier: the ground under it when it first sat down, where its altitude is 0 */
    float flAlt;          /* a plane: the altitude it heads for now, moving at climb_rate toward flWant */
    float flPitch;        /* a helicopter: its blades' share of their pitch, 0 flat on its gear to 1 flying */
    uint32_t rail;        /* a rail mover's rail, once it is on it (movers_rail.c) */
    float railS, railDir; /* how far along it, and +1 or -1: the way along it its heading points */
    uint32_t loggedDriver;
    double seatAge, sinceLog, logUntil;
    /* co-op (movers_sync.c): remote, another machine moves it and this is its copy; start, where it was first read */
    uint32_t remote, synced, peerEntity;
    uint64_t peerFrom;
    float start[3];
    double syncAge;
    MvWire sync;
    /* SDK 47 (movers_kit.c): its fixed guns and damage zones, by their slot on its mover */
    uint32_t kitInit, gunRounds[MV_KIT_SLOTS], gunBurst[MV_KIT_SLOTS], gunFiring[MV_KIT_SLOTS], gunSeated[MV_KIT_SLOTS];
    float gunWait[MV_KIT_SLOTS], znSink, znSpin, znSweep, znContact[MV_ZONE_SLOTS]; /* znSink, znSpin: m/s, rad/s a broken zone forces */
    uint8_t gunHeld[MV_KIT_SLOTS];
    /* co-op (movers_kit.c "@hellforge.guns"): a joiner's trigger on the host (seconds it holds), and what a joiner last sent */
    float gunRemote[MV_KIT_SLOTS][MV_GUN_SEATS], gunSent[MV_KIT_SLOTS]; /* gunRemote: seconds a joiner's seat stays taken */
    uint8_t gunRemoteHeld[MV_KIT_SLOTS][MV_GUN_SEATS], gunSentSeat[MV_KIT_SLOTS], gunSentHeld[MV_KIT_SLOTS], gunSeat[MV_KIT_SLOTS];
    int32_t znHealth[MV_ZONE_SLOTS];
    /* SDK 48 (movers_view.c): mouse_steer's throttle lever, whether this machine's Helldiver drives it, and A/D */
    float seatLever, seatKeys, viewSteer, viewLift; /* viewSteer, viewLift: the view's asks, eased in */
    uint32_t seatLocal;
} MvUnit;

static MvDef g_mvDefs[MV_DEFS];
static volatile LONG g_mvDefCount;
static MvUnit g_mvUnits[MV_UNITS];
static SRWLOCK g_mvLock = SRWLOCK_INIT;
static HfMod g_mvMod;
static volatile LONG g_mvTicking;
static MvPosFn g_mvPos, g_mvRot;
static MvLevelFn g_mvLevel, g_mvFromHandle;
static MvWaterFn g_mvWater;
static DWORD g_mvPushThread; /* the game thread while a mod's drive function runs: mover_* refuse rather than deadlock */

static float MvWrap(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static float MvClamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float MvToward(float v, float want, float dv) { return want > v + dv ? v + dv : want < v - dv ? v - dv : want; }

static MvUnit* MvUnitOf(uint32_t entity)
{
    for (int i = 0; i < MV_UNITS; i++) if (entity && g_mvUnits[i].entity == entity) return &g_mvUnits[i];
    return NULL;
}

/* ---- the ground ---- */

/* A hit unit that is solid: the level's own (rocks, props), or anything that is not a moving unit listed around it.
 * A ray can still hit a unit the game has just removed; UnitLevel reads its record unchecked, so that one is passed. */
static int MvSolid(const MvUnit* m, uint32_t unit)
{
    if (g_mvFromHandle && g_mvFromHandle(unit) == NULL) return 0;
    if (g_mvFromHandle && g_mvLevel && g_mvLevel(unit) != NULL) return 1;
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

/* A sound on the unit, as abilities post theirs (abilities.c: its game.dll base is set once anything posts). */
static void MvSound(uint32_t entity, uint32_t event)
{
    if (event == 0) return;
    if (!g_ablGame) g_ablGame = ApiModuleBase("game.dll", NULL);
    if (!g_bhvGame) g_bhvGame = g_ablGame;
    if (g_ablGame) AbilitySound(entity, event);
}

/* Start, stop and blocked sounds on its state's changes; the idle sound once at its start. */
static void MvSounds(MvUnit* m, const HfMoverSounds* s)
{
    const uint32_t moving = fabsf(m->speed) > 0.3f || fabsf(m->side) > 0.3f, blocked = m->status == HF_MOVER_BLOCKED;
    if (!m->sounded) { m->sounded = 1; m->wasMoving = moving; MvSound(m->entity, moving ? s->moving : s->idle); }
    else if (moving != m->wasMoving)
    {
        m->wasMoving = moving;
        MvSound(m->entity, moving ? s->start : s->stop);
        MvSound(m->entity, moving ? s->moving : s->idle);
    }
    if (blocked && !m->wasBlocked) MvSound(m->entity, s->blocked);
    m->wasBlocked = blocked;
}

/* The water surface over (x, y), or -500 for none (or no reviewed water query). */
static float MvWaterAt(float x, float y, float z)
{
    uintptr_t game = ApiModuleBase("game.dll", NULL), state, g, level;
    if (!g_mvWater || !game || !EvpPtr(game + MV_WATER_STATE, &state) || !EvpPtr(game + WC_GAME, &g) || !EvpPtr(g + MV_GAME_LEVEL, &level))
        return -500.0f;
    const float p[3] = {x, y, z}, w = g_mvWater((void*)state, p, (void*)level);
    return isfinite(w) ? w : -500.0f;
}

/* A floater in deep water: no deeper than its draft under the surface, within its steps of where it was. */
static int MvFloat(MvUnit* m, const HfMoverDef* d, float g, float w, float z, float* out)
{
    *out = fmaxf(g, w - d->wade_depth);
    if (*out - z > d->step_up || z - *out > d->step_down) return 0;
    m->waterDepth = w - g;
    return 1;
}

/* Where it stands at (x, y): the ground, with the water there as its water setting takes it. 1 with the height. A
 * floater's drop into deep water is no drop: the ground there is looked for straight down. */
static int MvSurface(MvUnit* m, const HfMoverDef* d, float x, float y, float z, float* out)
{
    float g;
    const int ok = MvGround(m, d, x, y, z, &g);
    const float w = d->water == HF_WATER_SINK ? -500.0f : MvWaterAt(x, y, z);
    m->waterDepth = 0;
    if (w < -499.0f) { if (ok) *out = g; return ok; }
    if (!ok)
    {
        const float top[3] = {x, y, z + 0.3f}, deep[3] = {x, y, z - 80.0f};
        if (d->water != HF_WATER_FLOAT) return 0;
        if (!MvRay(m, top, deep, &g)) g = w - 80.0f;
    }
    if (w <= g) { *out = g; return 1; }
    if (d->water == HF_WATER_FLOAT) return MvFloat(m, d, g, w, z, out);
    if (w - g > (d->water == HF_WATER_DRY ? 0.05f : d->wade_depth)) return 0;
    m->waterDepth = w - g;
    *out = g;
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

/* The incline along (ux, uy): the ground over MV_SLOPE_BASE or more ahead against the ground under it (its root may sit
 * off the ground: a pod drops it a little low). The reads leave waterDepth as the step's own surface set it. */
static float MvIncline(MvUnit* m, const HfMoverDef* d, float ux, float uy, float len, float g)
{
    const float base = fmaxf(len, MV_SLOPE_BASE), depth = m->waterDepth;
    float here = m->pos[2], ahead = g;
    if (!MvSurface(m, d, m->pos[0], m->pos[1], m->pos[2], &here)) here = m->pos[2];
    if (base > len && !MvSurface(m, d, m->pos[0] + ux * base, m->pos[1] + uy * base, m->pos[2], &ahead)) ahead = g;
    m->waterDepth = depth;
    return MvClamp((ahead - here) / base, -3, 3);
}

/* One step of `step` metres (negative: backward) along its heading; a walker also tries 35, 70 and 105 degrees to either
 * side. 1 moved. */
static int MvStep(MvUnit* m, const MvDef* def, float step)
{
    const HfMoverDef* d = &def->def;
    static const float kTry[] = {0.0f, 0.61f, -0.61f, 1.22f, -1.22f, 1.83f, -1.83f};
    const unsigned tries = d->mode == HF_MOVE_WALK && step > 0 ? 7u : 1u;
    const float fx = -sinf(m->yaw), fy = cosf(m->yaw), sign = step < 0 ? -1.0f : 1.0f, len = fabsf(step);
    for (unsigned i = 0; i < tries; i++)
    {
        const float c = cosf(kTry[i]), s = sinf(kTry[i]);
        const float ux = (fx * c - fy * s) * sign, uy = (fx * s + fy * c) * sign;
        float g;
        if (MvWall(m, d, ux, uy, len) || !MvSurface(m, d, m->pos[0] + ux * len, m->pos[1] + uy * len, m->pos[2], &g)) continue;
        /* a slope is a long incline, a step a sudden rise: averaged over MV_SLOPE_RUN of travel, a step passes */
        const float incline = MvIncline(m, d, ux, uy, len, g), k = fminf(1.0f, len / MV_SLOPE_RUN);
        const float climb = m->climb + k * (incline - m->climb);
        if (climb > def->slopeTan && incline > def->slopeTan) continue; /* still steep here: a stale average never pins it */
        m->climb = climb;
        m->grade = m->grade + k * (incline * sign - m->grade);
        m->pos[0] += ux * len; m->pos[1] += uy * len; m->pos[2] = g;
        return 1;
    }
    return 0;
}

/* A sideways slide of `dist` metres (positive: left), for strafing: no side tries, no slope beyond max_slope. */
static void MvSlide(MvUnit* m, const MvDef* def, float dist)
{
    const float ux = -cosf(m->yaw) * (dist < 0 ? -1.0f : 1.0f), uy = -sinf(m->yaw) * (dist < 0 ? -1.0f : 1.0f), len = fabsf(dist);
    float g;
    if (len < 0.0005f || MvWall(m, &def->def, ux, uy, len) || !MvSurface(m, &def->def, m->pos[0] + ux * len, m->pos[1] + uy * len, m->pos[2], &g) ||
        MvIncline(m, &def->def, ux, uy, len, g) > def->slopeTan)
    { m->side = 0; return; }
    m->pos[0] += ux * len; m->pos[1] += uy * len; m->pos[2] = g;
}

/* tilt_limit: pitch and roll from the ground half a metre ahead, behind and to either side, within tilt_limit. */
static void MvAlign(MvUnit* m, const HfMoverDef* d)
{
    const float fx = -sinf(m->yaw), fy = cosf(m->yaw), lim = d->tilt_limit * 0.0174533f;
    float z[4];
    static const float kAt[4][2] = {{0.5f, 0}, {-0.5f, 0}, {0, 0.5f}, {0, -0.5f}}; /* ahead, behind, left, right */
    for (int i = 0; i < 4; i++)
    {
        const float x = m->pos[0] + fx * kAt[i][0] - fy * kAt[i][1], y = m->pos[1] + fy * kAt[i][0] + fx * kAt[i][1];
        const float top[3] = {x, y, m->pos[2] + 1.2f}, low[3] = {x, y, m->pos[2] - 2.0f};
        if (!MvRay(m, top, low, &z[i])) z[i] = m->pos[2];
    }
    m->pitch += (MvClamp(atanf(z[0] - z[1]), -lim, lim) - m->pitch) * 0.3f;
    m->roll += (MvClamp(atanf(z[2] - z[3]), -lim, lim) - m->roll) * 0.3f;
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
static int MvFrame(MvUnit* m, float w[16])
{
    float l[16], wn[9], ln[9];
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
    return 1;
}

static int MvParent(MvUnit* m)
{
    float w[16];
    if (!MvFrame(m, w)) return 0;
    memcpy(m->pos, w + 12, 12);
    m->yaw = atan2f(-w[4], w[5]);
    m->written = 0;
    return 1;
}

/* Between its own moves the game may move it: a Pelican carries it, its hellpod lets go, something pushes it. When the
 * game shows the root away from where it was put, it goes on from where the game shows it. 0 when the root no longer reads. */
static int HvReady(void);
static uint32_t HvChassis(const MvUnit* m);

/* A physics body (a vehicle's) puts its unit back where it simulates it, so moves written to the root don't stick: twice in
 * a row while it drives and it is driven through its body instead, as a hover is. */
static void MvBodyUndoes(MvUnit* m, const MvDef* def)
{
    if (++m->undone < 2 || m->physics) return;
    m->body = HvReady() ? HvChassis(m) : 0;
    m->physics = m->body != 0;
    if (m->physics) ApiLog(def->mod, "mover: unit %u: its physics body undoes HellForge's moves; it is driven through the body (actor %08x)", m->entity, m->body);
    else if (m->undone == 2) ApiLog(def->mod, "mover: unit %u: something undoes HellForge's moves, and it has no physics body to push", m->entity);
}

static int MvKeepFrame(MvUnit* m, const MvDef* def)
{
    float w[16];
    if (!ApiJointPose(&g_mvMod, m->unit, 0, HF_POSE_WORLD, w)) return 0;
    const float dx = w[12] - m->put[0], dy = w[13] - m->put[1], dz = w[14] - m->put[2];
    if (!m->written || dx * dx + dy * dy + dz * dz < 0.25f) { if (m->written) m->undone = 0; return 1; }
    if (fabsf(m->speed) > 0.3f || fabsf(m->turning) > 0.1f) MvBodyUndoes(m, def);
    if (m->reframed++ < 3)
        ApiLog(def->mod, "mover: unit %u shown %.2f m from where it was put: the game moved it, it goes on from there", m->entity,
            (double)sqrtf(dx * dx + dy * dy + dz * dz));
    return MvParent(m);
}

/* Written only when it moved or turned: a root held still is left to its parent, as the game holds it. */
static void MvSetRoot(MvUnit* m)
{
    const float tilt = m->pitch * 3.0f + m->roll; /* any change of either */
    if (m->written && m->put[0] == m->pos[0] && m->put[1] == m->pos[1] && m->put[2] == m->pos[2] && m->put[3] == m->yaw + tilt) return;
    memcpy(m->put, m->pos, 12); m->put[3] = m->yaw + tilt; m->written = 1;
    const float d[3] = {m->pos[0] - m->par[3][0], m->pos[1] - m->par[3][1], m->pos[2] - m->par[3][2]};
    float local[3];
    for (int i = 0; i < 3; i++) local[i] = d[0] * m->par[i][0] + d[1] * m->par[i][1] + d[2] * m->par[i][2];
    /* heading, then pitch about its right axis, then roll about its forward axis (x, y, z, w) */
    const float h = (m->yaw - m->parYaw) * 0.5f, p = m->pitch * 0.5f, r = m->roll * 0.5f;
    const float zw = cosf(h), zz = sinf(h), px = sinf(p), pw = cosf(p), ry = sinf(r), rw = cosf(r);
    const float a[4] = {zw * px, zz * px, zz * pw, zw * pw}; /* yaw x pitch */
    const float q[4] = {a[0] * rw - a[2] * ry, a[3] * ry + a[1] * rw, a[2] * rw + a[0] * ry, a[3] * rw - a[1] * ry};
    g_mvPos(m->unit, 0, local);
    g_mvRot(m->unit, 0, q);
}

#include "movers_sync.c" /* co-op: the poses of walkers and drivers for the other machines (MvSetRoot, MvFrame) */

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
    /* backing off: it faces away from the point (toward the enemy) and drives in reverse */
    const float back = AcReversing(m->entity) ? -1.0f : 1.0f;
    const float err = MvWrap(atan2f(-dx * back, dy * back) - m->yaw), spot = 0.6f;
    const float close = MvClamp(dist / def->arrive, 0.25f, 1.0f) * back;
    if (def->def.turn_radius == 0)
    {
        m->steer = MvClamp(err / spot, -1.0f, 1.0f);
        m->throttle = fabsf(err) > spot ? 0.0f : close * (1.0f - fabsf(err) / (spot * 2.0f));
        return;
    }
    m->steer = MvClamp(err / 0.5f * back, -1.0f, 1.0f); /* in reverse a wheeled steer turns the nose the other way */
    m->throttle = fabsf(err) > 1.7f ? 0.35f * back : close; /* behind it: a slow wide turn */
}

static void MvSnapshot(const MvUnit* m, HfMoverState* s);

/* HfMoverDef.drive's push for this update. */
static HfMoverPush MvPush(MvUnit* m, const MvDef* def, float dt)
{
    HfMoverPush push = {sizeof push, 0, 0, 0};
    if (def->def.drive == NULL) return push;
    HfMoverState s;
    MvSnapshot(m, &s);
    g_mvPushThread = GetCurrentThreadId();
    def->def.drive(def->mod, m->entity, &s, dt, &push, def->def.drive_user);
    g_mvPushThread = 0;
    if (!isfinite(push.forward) || !isfinite(push.side) || !isfinite(push.turn)) memset(&push.forward, 0, 12);
    return push;
}

/* The fastest turn at this speed: turn_rate, or for a vehicle no tighter than turn_radius (backing up turns it back). */
static float MvRate(const HfMoverDef* d, float speed)
{
    const float rate = d->turn_rate * 0.0174533f;
    return d->turn_radius > 0 ? fminf(rate, fabsf(speed) / d->turn_radius) * (speed < 0 ? -1.0f : 1.0f) : rate;
}

/* A driven axle's share of the weight on a grade. The load moves to the rear by about (centre of mass height /
 * wheelbase) x (a/g + sin slope), 0.25 x for a car, so a rear drive climbs best and a front drive worst. */
static float MvDrivenShare(uint32_t axles, float grade, float accel)
{
    const float rear = MvClamp(0.5f + 0.25f * (accel / MV_G + grade / sqrtf(1 + grade * grade)), 0.1f, 0.9f);
    return axles == (HF_AXLE_FRONT | HF_AXLE_REAR) ? 1.0f : axles == HF_AXLE_REAR ? rear : 1.0f - rear;
}

/* The speed after dt toward `want`: acceleration speeding up, brake slowing down or turning about. With a drivetrain,
 * the tyres' grip caps both, and a grade adds gravity's pull along it. */
static float MvSpeedToward(const MvUnit* m, const MvDef* def, float want, float top, float dt)
{
    const HfMoverDef* d = &def->def;
    const float v = m->speed;
    const int slowing = fabsf(want) < fabsf(v) || want * v < 0;
    float a = slowing ? d->brake : d->acceleration;
    if (def->axles)
    {
        const float c = 1.0f / sqrtf(1 + m->grade * m->grade), dir = want > v ? 1.0f : -1.0f;
        const float grip = d->grip * MV_G * c; /* every wheel brakes; only the driven ones push */
        a = slowing ? fminf(a, grip) : fminf(a, grip * MvDrivenShare(def->axles, m->grade * dir, a * dir));
        a -= MV_G * m->grade * c * dir; /* uphill takes some of it, downhill adds */
        if (a < 0 && fabsf(v) < 0.05f) return 0; /* too steep for its grip: it holds where it is */
    }
    const float dv = a * dt; /* negative when a grade beats its grip or brakes: it loses ground or runs away downhill */
    return MvClamp(MvToward(v, want, dv), -d->reverse_speed, top);
}

/* Boost: boost_speed while the input holds and some is left; it refills at half the rate it is used. */
static float MvTop(MvUnit* m, const HfMoverDef* d, float dt)
{
    const int was = m->boosting;
    m->boosting = d->boost_speed > 0 && m->boost > 0.5f && m->boostLeft > 0 && m->throttle > 0.05f;
    m->boostLeft = m->boosting ? m->boostLeft - dt : fminf(d->boost_time, m->boostLeft + dt * 0.5f);
    if (m->boosting && !was) MvSound(m->entity, d->sounds.boost);
    return m->boosting ? d->boost_speed : d->speed;
}

static void MvBlocked(MvUnit* m, const MvDef* def)
{
    m->speed = 0;
    m->status = HF_MOVER_BLOCKED;
    if (m->blockedLogged++ == 0)
        ApiLog(def->mod, "mover: unit %u blocked at %.1f %.1f %.1f (a step it can't take, a wall, a slope or water too deep)", m->entity,
            (double)m->pos[0], (double)m->pos[1], (double)m->pos[2]);
}

/* A walker's sideways speed toward strafe x strafe_speed, then its slide. */
static void MvStrafe(MvUnit* m, const MvDef* def, float dt)
{
    const HfMoverDef* d = &def->def;
    const float want = m->strafe * d->strafe_speed, dv = (fabsf(want) > fabsf(m->side) ? d->acceleration : d->brake) * dt;
    m->side = MvToward(m->side, want, dv);
    MvSlide(m, def, m->side * dt);
}

/* Speed and heading from the inputs (or a mod's push), then the step over the ground and a walker's sideways slide. */
/* Speed, turn and (a walker's, from a mod's push) sideways speed from the inputs or the mod's drive function. */
static void MvSpeedTurn(MvUnit* m, const MvDef* def, float top, float dt)
{
    const HfMoverDef* d = &def->def;
    if (d->drive != NULL)
    {
        const HfMoverPush push = MvPush(m, def, dt);
        const float rate = fabsf(MvRate(d, m->speed));
        m->speed = MvClamp(m->speed + push.forward * dt, -d->reverse_speed, top);
        m->turning = MvClamp(push.turn, -rate, rate);
        if (d->mode == HF_MOVE_WALK) m->side = MvClamp(m->side + push.side * dt, -d->strafe_speed, d->strafe_speed);
        return;
    }
    if (def->asked == HF_MOVE_TRACKS) top *= 1.0f - 0.5f * fabsf(m->steer); /* the inner track brakes to turn */
    m->speed = MvSpeedToward(m, def, m->throttle >= 0 ? m->throttle * top : m->throttle * d->reverse_speed, top, dt);
    m->turning = m->steer * MvRate(d, m->speed);
}

#include "movers_rail.c" /* SDK 43: rails (MvSpeedTurn, MvTop, MvBlocked) */

static void MvDrive(MvUnit* m, const MvDef* def, float dt)
{
    const HfMoverDef* d = &def->def;
    if (d->mode == HF_MOVE_RAIL) { MvRailDrive(m, def, dt); return; }
    const float top = MvTop(m, d, dt);
    MvSpeedTurn(m, def, top, dt);
    m->driven = fabsf(m->throttle) > 0.02f ? def->axles : 0;
    m->yaw = MvWrap(m->yaw + m->turning * dt);
    m->status = HF_MOVER_MOVING;
    if (d->mode == HF_MOVE_WALK && d->drive == NULL) MvStrafe(m, def, dt);
    else if (d->mode == HF_MOVE_WALK) MvSlide(m, def, m->side * dt);
    if (fabsf(m->speed * dt) >= 0.0005f && !MvStep(m, def, m->speed * dt)) MvBlocked(m, def);
    else m->blockedLogged = 0;
    if (d->tilt_limit > 0) MvAlign(m, d);
}

/* Its first half minute every 2 s: where HellForge puts its root against where the game shows it, to see a move go wrong. */
static void MvTrace(MvUnit* m, const MvDef* def, float dt)
{
    if (m->age > def->def.start_delay + 30 || (m->sinceTrace += dt) < 2.0) return;
    m->sinceTrace = 0;
    float w[16];
    const int seen = ApiJointPose(&g_mvMod, m->unit, 0, HF_POSE_WORLD, w);
    ApiLog(def->mod, "mover: unit %u (game unit %u) put at %.2f %.2f %.2f yaw %.0f, shown %s %.2f %.2f %.2f; speed %.2f, throttle %.2f, steer %.2f, status %u",
        m->entity, m->unit, (double)m->pos[0], (double)m->pos[1], (double)m->pos[2], (double)(m->yaw * 57.29578f), seen ? "at" : "nowhere",
        (double)(seen ? w[12] : 0), (double)(seen ? w[13] : 0), (double)(seen ? w[14] : 0), (double)m->speed, (double)m->throttle,
        (double)m->steer, m->status);
}

#define HV_SEAT_S 2.0 /* a hover's seated driver's last throttle or steer drives it this long */

static void ViewSteer(MvUnit* m, const MvDef* def, float dt); /* movers_view.c (SDK 48) */
/* Who drives it now: a mod's input while it holds, a hover's seated driver, the custom AI, or nobody. */
static void MvDriver(MvUnit* m, const MvDef* def, float dt)
{
    if (m->inputAge < MV_INPUT_S) { m->driver = HF_DRIVER_MOD; return; }
    m->strafe = m->lift = m->boost = 0; /* a mod's input has them, and a seat's lift keys */
    if (m->seatAge < HV_SEAT_S) /* any mover whose unit has a driver's seat, read where it is moved */
    {
        m->driver = HF_DRIVER_SEAT; m->throttle = m->seatThrottle; m->steer = m->seatSteer; m->lift = m->seatLift;
        if (def->def.mouse_steer && m->seatLocal) ViewSteer(m, def, dt); /* SDK 48: toward where the camera looks */
    }
    else if (def->ai) { m->driver = HF_DRIVER_AI; MvAiInputs(m, def); }
    else { m->driver = HF_DRIVER_NONE; m->throttle = 0; m->steer = 0; }
}

#include "movers_hover.c" /* SDK 36: hover vehicles (MvUnit, MvRay, MvWaterAt, MvPush, MvRate) */

/* A rail mover whose physics body undid its moves (a vehicle copy): the line runs as ever, and the body is pushed along
 * it at its speed and back onto it, turned toward the track's heading. Its height stays the body's. */
static void MvBodyRail(MvUnit* m, const MvDef* def, const float w[16], const float v[3], const float om[3], float dt)
{
    const float body[2] = {w[12], w[13]}, bodyYaw = atan2f(-w[4], w[5]);
    MvDriver(m, def, dt);
    MvRailDrive(m, def, dt);
    if (!m->rail) return;
    const float fx = -sinf(m->yaw), fy = cosf(m->yaw), k = fminf(1.0f, 8.0f * dt), mass = def->def.hover.mass > 0 ? def->def.hover.mass : 3000.0f;
    const float want[2] = {fx * m->speed + (m->pos[0] - body[0]) * 2.0f, fy * m->speed + (m->pos[1] - body[1]) * 2.0f};
    const float j[3] = {(want[0] - v[0]) * mass * k, (want[1] - v[1]) * mass * k, 0};
    const float t[3] = {0, 0, (MvWrap(m->yaw - bodyYaw) * 3.0f - om[2]) * mass * 2.0f * k};
    g_hvImpulse(m->body, j, NULL);
    g_hvAngular(m->body, t);
    MvSounds(m, &def->def.sounds);
    MvTrace(m, def, dt);
}

/* A tracked vehicle on a wheeled body pivots against its tyres' sideways grip: the yaw impulse its tracks would add to
 * beat that skid, grip x weight on a 1.5 m half-track lever, while it turns slower than it wants. */
static float MvSkid(const MvUnit* m, const MvDef* def, float yawRate, float mass, float dt)
{
    const float miss = m->turning - yawRate;
    if (def->asked != HF_MOVE_TRACKS || fabsf(m->turning) < 0.05f || fabsf(miss) < 0.05f) return 0;
    return copysignf(def->def.grip * mass * MV_G * 1.5f * dt, miss);
}

/* A walker or driver whose physics body undid its moves: the body is pushed toward the speed, sideways speed and turn
 * HellForge wants, as tyres or feet would; the ground, walls and its height stay the body's own. */
static void MvBodyTick(MvUnit* m, const MvDef* def, float dt)
{
    const HfMoverDef* d = &def->def;
    float w[16], v[3] = {0}, om[3] = {0};
    if (!g_hvResolves(m->body) || g_hvUnit(m->body) != m->unit) { m->physics = 0; m->undone = 0; m->started = 0; return; }
    if (!ApiJointPose(&g_mvMod, m->unit, 0, HF_POSE_WORLD, w) || !isfinite(w[12])) return;
    g_hvVelocity(m->body, v, om);
    memcpy(m->pos, w + 12, 12);
    m->yaw = atan2f(-w[4], w[5]);
    if (d->mode == HF_MOVE_RAIL) { MvBodyRail(m, def, w, v, om, dt); return; }
    MvDriver(m, def, dt);
    const float top = MvTop(m, d, dt);
    MvSpeedTurn(m, def, top, dt);
    if (d->drive == NULL) m->side = d->mode == HF_MOVE_WALK ? m->strafe * d->strafe_speed : 0;
    m->driven = fabsf(m->throttle) > 0.02f ? def->axles : 0;
    const float fx = -sinf(m->yaw), fy = cosf(m->yaw), k = fminf(1.0f, 8.0f * dt), mass = d->hover.mass > 0 ? d->hover.mass : 3000.0f;
    const float want[2] = {fx * m->speed - fy * m->side, fy * m->speed + fx * m->side};
    const float j[3] = {(want[0] - v[0]) * mass * k, (want[1] - v[1]) * mass * k, 0};
    const float t[3] = {0, 0, (m->turning - om[2]) * mass * 2.0f * k + MvSkid(m, def, om[2], mass, dt)}; /* about 2 m^2 x mass: a car's yaw inertia */
    g_hvImpulse(m->body, j, NULL);
    g_hvAngular(m->body, t);
    const float got = v[0] * fx + v[1] * fy;
    m->status = fabsf(m->speed) > 0.5f && fabsf(got) < 0.1f * fabsf(m->speed) ? HF_MOVER_BLOCKED : HF_MOVER_MOVING;
    MvSounds(m, &d->sounds);
    MvTrace(m, def, dt);
}

/* A hover, a walker or driver its body drives, or another machine's copy: 1 when it had its update here. */
static int MvBodyModes(MvUnit* m, const MvDef* def, float dt)
{
    if (m->remote) { MvSyncTick(m, dt); return 1; }
    if (MvPhysical(def->def.mode)) { HvUnitTick(m, def, dt); return 1; }
    if (!m->physics || !m->started) return 0;
    MvBodyTick(m, def, dt);
    return 1;
}

static void KitTick(MvUnit* m, float dt); /* movers_kit.c */
static void KitTake(void);               /* movers_kit.c */

static void MvUnitTick(MvUnit* m, float dt)
{
    const MvDef* def = &g_mvDefs[m->def - 1];
    m->age += dt;
    m->inputAge += dt;
    m->seatAge += dt;
    uint32_t unit = 0, network = 0;
    if (!WcEntity(m->entity, &unit, &network) || !unit) return;
    if (unit != m->unit) /* a hellpod's payload can change units as it deploys: its root is read again */
    {
        if (m->unit) ApiLog(def->mod, "mover: unit %u is now game unit %u (was %u): its root is read again", m->entity, unit, m->unit);
        m->unit = unit; m->started = 0;
    }
    if (m->age < def->def.start_delay) { m->status = HF_MOVER_WAITING; return; }
    if (MvBodyModes(m, def, dt)) return;
    if (!m->started)
    {
        if (!MvParent(m)) { m->status = HF_MOVER_WAITING; return; }
        m->started = 1;
        memcpy(m->start, m->pos, 12);
        ApiLog(def->mod, "mover: unit %u moved by HellForge from %.1f %.1f %.1f", m->entity, (double)m->pos[0], (double)m->pos[1], (double)m->pos[2]);
    }
    if ((m->sinceSolid += dt) >= MV_SOLID_S)
    {
        m->sinceSolid = 0;
        MvListMobile(m);
        if (!MvKeepFrame(m, def)) { ApiLog(def->mod, "mover: unit %u: its root no longer reads; it waits", m->entity); m->started = 0; return; }
    }
    if (!m->written && !MvParent(m)) { m->status = HF_MOVER_WAITING; return; } /* still the game's: where it shows it now */
    MvDriver(m, def, dt);
    MvDrive(m, def, dt);
    MvSounds(m, &def->def.sounds);
    MvSetRoot(m);
    MvTrace(m, def, dt);
}

/* The game's calls resolve only once the game has loaded (their code may decode later still), and mods add movers before
 * that: ask each update until they come, and say so once after a minute without them. */
static int MvCalls(float dt)
{
    static float waited;
    static int said;
    if (g_mvPos && g_mvRot) return 1;
    if (said || !g_fired[HF_EVENT_GAME_LOADED]) return 0;
    g_mvPos = (MvPosFn)ApiFunction(&g_mvMod, "UnitTeleportLocalPosition");
    g_mvRot = (MvPosFn)ApiFunction(&g_mvMod, "UnitTeleportLocalRotation");
    if (!g_mvLevel) g_mvLevel = (MvLevelFn)ApiFunction(&g_mvMod, "UnitLevel");
    if (!g_mvFromHandle) g_mvFromHandle = (MvLevelFn)ApiFunction(&g_mvMod, "UnitFromHandle");
    if (!g_mvWater) g_mvWater = (MvWaterFn)ApiFunction(&g_mvMod, "WaterHeight");
    if (g_mvPos && g_mvRot) { Note(g_mvWater ? "movers: on" : "movers: on, without water (WaterHeight is not the reviewed code)"); return 1; }
    if ((waited += dt) > 60 && !said) { said = 1; Note("movers: off; the teleport calls are not the reviewed ones"); }
    return 0;
}

static void MvTick(HfMod* mod, float dt, void* user)
{
    (void)mod; (void)user;
    if (!MvCalls(dt)) return;
    AcquireSRWLockExclusive(&g_mvLock);
    MvSyncTake();
    KitTake(); /* joiners' fixed-gun triggers (movers_kit.c) */
    for (int i = 0; i < MV_UNITS; i++)
    {
        MvUnit* m = &g_mvUnits[i];
        if (!m->entity) continue;
        if (!EvpAlive(m->entity))
        {
            ApiLog(g_mvDefs[m->def - 1].mod, "mover: unit %u is gone after %.0f s, last put at %.1f %.1f %.1f", m->entity, m->age,
                (double)m->pos[0], (double)m->pos[1], (double)m->pos[2]);
            memset(m, 0, sizeof *m);
            continue;
        }
        MvUnitTick(m, dt);
        KitTick(m, dt);
    }
    MvSyncSend(dt);
    ReleaseSRWLockExclusive(&g_mvLock);
}

static void MvSpawned(HfMod* mod, const HfSpawnEvent* e, void* user)
{
    (void)mod; (void)user;
    for (LONG d = 0; d < g_mvDefCount; d++)
    {
        /* every machine follows it: a hover where it is simulated (a client may drive it), the others as copies */
        if (g_mvDefs[d].def.entity != e->resource) continue;
        AcquireSRWLockExclusive(&g_mvLock);
        for (int i = 0; i < MV_UNITS; i++)
            if (!g_mvUnits[i].entity)
            {
                MvUnit* m = &g_mvUnits[i];
                memset(m, 0, sizeof *m);
                m->entity = e->entity; m->def = (uint32_t)d + 1;
                m->boostLeft = g_mvDefs[d].def.boost_time; m->targetHeight = g_mvDefs[d].def.hover.height;
                m->seatAge = 1e9; /* no seat has driven it: the lift keys wait for one */
                m->remote = !e->authority && !MvPhysical(g_mvDefs[d].def.mode);
                break;
            }
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
    ApiSubscribe(&g_mvMod, HF_ON_SPAWN, (HfListener)(void (*)(void))MvSpawned, NULL, 0);
    ApiOnGameThread(&g_mvMod, MvTick, NULL);
}

/* 1 with the mover's position when this machine moves the entity (ai_orders.c: units with no brain record). */
static int MvWhere(uint32_t entity, float out[3])
{
    if (!g_mvDefCount || !TryAcquireSRWLockShared(&g_mvLock)) return 0;
    const MvUnit* m = MvUnitOf(entity);
    int ok = m != NULL && m->started && !m->remote;
    if (ok) memcpy(out, m->pos, 12);
    ReleaseSRWLockShared(&g_mvLock);
    return ok;
}

/* 1 while the entity is a mover's unit whose root HellForge has not read yet (its start_delay). */
static int MvPending(uint32_t entity)
{
    if (!g_mvDefCount || !TryAcquireSRWLockShared(&g_mvLock)) return 0;
    const MvUnit* m = MvUnitOf(entity);
    int pending = m != NULL && !m->started && !m->remote;
    ReleaseSRWLockShared(&g_mvLock);
    return pending;
}

/* 1 while a mover's unit has not started, or turns on the spot: no headway is expected of it yet (ai_orders.c). */
static int MvSettling(uint32_t entity)
{
    if (!g_mvDefCount || !TryAcquireSRWLockShared(&g_mvLock)) return 0;
    const MvUnit* m = MvUnitOf(entity);
    int settling = m != NULL && !m->remote && (!m->started || (fabsf(m->turning) > 0.05f && fabsf(m->speed) < 0.2f));
    ReleaseSRWLockShared(&g_mvLock);
    return settling;
}

/* ---- the API ---- */

/* Hover defaults are the tested HV-1's: 0.3 g of fan thrust and 22 m/s, a light hovercraft's; reversed fans brake as
 * hard as they push. */
static void MvOr(float* v, float fallback) { if (*v == 0) *v = fallback; }

static void MvHoverDefaults(HfMoverHover* h)
{
    MvOr(&h->height, 1.5f);
    MvOr(&h->height_min, h->height);
    MvOr(&h->height_max, h->height);
    MvOr(&h->climb_rate, 2.0f);
    MvOr(&h->mass, 3000.0f);
    MvOr(&h->stiffness, 1.2f);
    MvOr(&h->damping, 0.7f);
    MvOr(&h->side_drag, 0.5f);
}

/* A flier sits height_min up on its gear and flies height over the ground: 10 m for a helicopter, a plane's 40 m
 * circuit; heavier than the hover, a light helicopter's or plane's mass. */
static void MvFlierDefaults(HfMoverHover* h, int plane)
{
    MvOr(&h->height, plane ? 40.0f : 10.0f);
    MvOr(&h->height_min, 1.0f);
    MvOr(&h->height_max, 4.0f * h->height);
    MvOr(&h->climb_rate, plane ? 8.0f : 5.0f);
    MvOr(&h->mass, plane ? 1100.0f : 2500.0f);
    MvOr(&h->side_drag, plane ? 3.0f : 0.8f);
}

/* A vessel's hull: a patrol boat's or small submarine's mass, a keel against sideways slides, and a dive rate. */
static void MvVesselDefaults(HfMoverHover* h, int sub)
{
    MvOr(&h->mass, sub ? 8000.0f : 4000.0f);
    MvOr(&h->side_drag, 2.0f);
    MvOr(&h->climb_rate, sub ? 0.5f : 2.0f);
    MvOr(&h->stiffness, 0.6f);
}

/* Per kind: top speed, acceleration, turn rate, strafe (0: half of speed), wade depth or draft, turn radius. A walker's
 * pace and mid-thigh wading; an SUV's wading depth; HV-1's hovercraft; a utility helicopter's 40 m/s and a light
 * plane's 60 m/s cruise on a nose wheel; a patrol boat's 12 m/s (23 knots) and a small submarine's 7 m/s on a rudder. */
static const float kMvKind[][6] = {
    {4.0f, 8.0f, 120.0f, 0, 0.8f, 0},       /* walk */
    {4.0f, 8.0f, 120.0f, 0, 0.5f, 0},       /* drive (and its drivetrains) */
    {22.0f, 3.0f, 57.0f, 5.0f, 0.8f, 0},    /* hover */
    {40.0f, 5.0f, 45.0f, 10.0f, 0.5f, 0},   /* helicopter */
    {60.0f, 4.0f, 30.0f, 0, 0.5f, 8.0f},    /* plane */
    {12.0f, 1.5f, 20.0f, 0, 0.5f, 12.0f},   /* boat */
    {7.0f, 0.8f, 12.0f, 0, 1.5f, 15.0f},    /* submarine */
    {15.0f, 1.0f, 1.0f, 0, 0.5f, 0},       /* rail: light rail's 54 km/h, a service brake's 1 m/s2 */
    {10.0f, 2.0f, 40.0f, 0, 1.2f, 0},      /* tracks: a tank's 36 km/h across country, its 1.2 m fording */
};

static void MvDefaults(HfMoverDef* d)
{
    static const int kKindOf[] = {0, 0, 1, 2, 1, 1, 1, 3, 4, 5, 6, 7, 8};
    if (!d->mode) d->mode = HF_MOVE_WALK;
    const float* k = kMvKind[d->mode <= HF_MOVE_TRACKS ? kKindOf[d->mode] : 0];
    if (!d->water) d->water = HF_WATER_WADE;
    MvOr(&d->speed, k[0]);
    if (d->reverse_speed < 0) d->reverse_speed *= -d->speed; /* a share of its forward speed */
    MvOr(&d->acceleration, k[1]);
    MvOr(&d->brake, d->acceleration);
    MvOr(&d->wade_depth, k[4]);
    MvOr(&d->grip, d->mode == HF_MOVE_TRACKS ? 1.0f : 0.7f); /* steel or rubber tracks on dirt */
    MvOr(&d->turn_rate, k[2]);
    MvOr(&d->turn_radius, k[5]);
    MvOr(&d->step_up, 0.5f);
    MvOr(&d->step_down, 1.0f);
    MvOr(&d->reach, 0.6f);
    MvOr(&d->max_slope, atanf(d->step_up) * 57.29578f); /* step_up risen over 1 m */
    MvOr(&d->boost_time, 3.0f);
    MvOr(&d->strafe_speed, k[3] > 0 ? k[3] : fminf(d->speed * 0.5f, 40.0f)); /* the refusal's top: a fast plane's half would be over it */
    MvOr(&d->stall_speed, 0.45f * d->speed);
    MvOr(&d->bank_max, 40.0f);
    MvOr(&d->dive_depth, 3.0f);
    MvOr(&d->thrust_max, 1.3f); /* a utility helicopter's, its engine's limit */ /* periscope depth: the game draws nothing under water */
    if (MvFlier(d->mode)) MvFlierDefaults(&d->hover, d->mode == HF_MOVE_PLANE);
    if (MvVessel(d->mode)) MvVesselDefaults(&d->hover, d->mode == HF_MOVE_SUB);
    MvHoverDefaults(&d->hover);
}

static const char* MvHoverRefusal(const HfMoverHover* h, int flier)
{
    const float top = flier ? 500.0f : 25.0f;
    if (!AiIn(h->height_min, 0.2f, top) || !AiIn(h->height_max, h->height_min, top) || !AiIn(h->height, h->height_min, h->height_max) ||
        !AiIn(h->climb_rate, 0.1f, 50) || !AiIn(h->carried, 0, 1) || !AiIn(h->mass, 50, 200000) || !AiIn(h->stiffness, 0.2f, 5) ||
        !AiIn(h->damping, 0.1f, 2) || !AiIn(h->side_drag, 0, 20))
        return "hover: heights 0.2 .. 25 (a flier 500) with height between min and max, climb_rate 0.1 .. 50, carried 0 .. 1, mass 50 .. 200000, stiffness 0.2 .. 5, damping 0.1 .. 2, side_drag 0 .. 20";
    return NULL;
}

/* stall_speed only matters to a plane: its default, 0.45 x speed, is under 1 for a slow ground mover. */
static int MvSpeedsIn(const HfMoverDef* d)
{
    return AiIn(d->speed, 0.1f, MvFlier(d->mode) ? 120.0f : 40.0f) && (d->mode != HF_MOVE_PLANE || AiIn(d->stall_speed, 1, d->speed)) &&
        AiIn(d->bank_max, 5, 80) && AiIn(d->dive_depth, 0.5f, 30);
}

static const char* MvRefusal(const HfMoverDef* in, HfMoverDef* d)
{
    if (in == NULL || in->size < HF_MOVER_DEF_SIZE_39 || in->entity == 0) return "no HfMoverDef, its size (build against SDK 39's header or later) or its entity";
    memset(d, 0, sizeof *d);
    memcpy(d, in, in->size < sizeof *d ? in->size : sizeof *d);
    MvDefaults(d);
    if (d->mode > HF_MOVE_TRACKS || d->water > HF_WATER_SINK) return "mode WALK, DRIVE (_FWD, _RWD, _4WD), HOVER, HELI, PLANE, BOAT, SUB, RAIL or TRACKS, water WADE, DRY, FLOAT or SINK";
    if (d->rail > MV_RAILS) return "rail: an id add_rail gave, or 0 for the nearest";
    if (!MvSpeedsIn(d)) return "speed 0.1 .. 40 (a flier 120), a plane's stall_speed 1 .. speed, bank_max 5 .. 80, dive_depth 0.5 .. 30";
    if (!AiIn(d->thrust_max, 1.05f, 3)) return "thrust_max 1.05 .. 3";
    if (d->mouse_steer > 1) return "mouse_steer 0 or 1";
    if (!AiIn(d->reverse_speed, 0, 40) || !AiIn(d->acceleration, 0.1f, 200) || !AiIn(d->brake, 0.1f, 200) ||
        !AiIn(d->turn_rate, 1, 1440) || !AiIn(d->turn_radius, 0, 100) || !AiIn(d->step_up, 0, 10) || !AiIn(d->step_down, 0, 50) ||
        !AiIn(d->reach, 0, 20) || !AiIn(d->start_delay, 0, 120))
        return "reverse 0 .. 40 (or -1 .. 0 of speed), acceleration and brake 0.1 .. 200, turn_rate 1 .. 1440, turn_radius 0 .. 100, steps 0 .. 10 up and 50 down, reach 0 .. 20, start_delay 0 .. 120";
    if (!AiIn(d->wade_depth, 0, 20) || !AiIn(d->grip, 0.05f, 2) || !AiIn(d->max_slope, 1, 90) || !AiIn(d->tilt_limit, 0, 90) ||
        !AiIn(d->boost_speed, 0, 60) || !AiIn(d->boost_time, 0.1f, 600) || !AiIn(d->strafe_speed, 0, 40))
        return "wade_depth 0 .. 20, grip 0.05 .. 2, max_slope 1 .. 90, tilt_limit 0 .. 90, boost_speed 0 .. 60, boost_time 0.1 .. 600, strafe_speed 0 .. 40";
    return MvHoverRefusal(&d->hover, MvFlier(d->mode));
}

/* SDK 54: what a redefinition changed, as "speed 12 -> 14, hover.height 2 -> 3" (or "nothing"). */
static void MvChanges(const HfMoverDef* was, const HfMoverDef* now, char* out, size_t size)
{
    static const struct { const char* name; size_t at; } kFields[] = {
        {"speed", offsetof(HfMoverDef, speed)}, {"reverse_speed", offsetof(HfMoverDef, reverse_speed)},
        {"acceleration", offsetof(HfMoverDef, acceleration)}, {"brake", offsetof(HfMoverDef, brake)},
        {"turn_rate", offsetof(HfMoverDef, turn_rate)}, {"turn_radius", offsetof(HfMoverDef, turn_radius)},
        {"grip", offsetof(HfMoverDef, grip)}, {"max_slope", offsetof(HfMoverDef, max_slope)}, {"tilt_limit", offsetof(HfMoverDef, tilt_limit)},
        {"boost_speed", offsetof(HfMoverDef, boost_speed)}, {"strafe_speed", offsetof(HfMoverDef, strafe_speed)},
        {"stall_speed", offsetof(HfMoverDef, stall_speed)}, {"bank_max", offsetof(HfMoverDef, bank_max)},
        {"thrust_max", offsetof(HfMoverDef, thrust_max)}, {"hover.height", offsetof(HfMoverDef, hover.height)},
        {"hover.height_min", offsetof(HfMoverDef, hover.height_min)}, {"hover.height_max", offsetof(HfMoverDef, hover.height_max)},
        {"hover.climb_rate", offsetof(HfMoverDef, hover.climb_rate)}, {"hover.mass", offsetof(HfMoverDef, hover.mass)},
        {"hover.stiffness", offsetof(HfMoverDef, hover.stiffness)}, {"hover.damping", offsetof(HfMoverDef, hover.damping)}};
    size_t n = 0;
    out[0] = 0;
    for (size_t i = 0; i < sizeof kFields / sizeof kFields[0] && n + 1 < size; i++)
    {
        float a = *(const float*)((const char*)was + kFields[i].at), b = *(const float*)((const char*)now + kFields[i].at);
        if (a == b) continue;
        int w = snprintf(out + n, size - n, "%s%s %g -> %g", n ? ", " : "", kFields[i].name, (double)a, (double)b);
        n = w > 0 && (size_t)w < size - n ? n + (size_t)w : size - 1;
    }
    if (n == 0) snprintf(out, size, "nothing it logs");
}

/* SDK 54: add_mover again for an entity the mod moves already, on the game thread: its definition replaced in its slot,
 * so its units keep their state. Its mode stays (a hover's body and a walker's steps are not swapped live). Under the
 * add lock. NULL done, else why refused. */
static const char* MvRedefine(HfMod* mod, LONG slot, const HfMoverDef* d, uint32_t asked, uint32_t axles, char* changes, size_t size)
{
    MvDef* m = &g_mvDefs[slot];
    if (m->mod != mod && !ModGone(m->mod)) return "it has a mover already (another mod's)";
    if (g_gameThread == 0 || GetCurrentThreadId() != g_gameThread) return "it has a mover already (redefine it on the game thread)";
    if (m->asked != asked) return "a redefinition keeps its mode";
    MvChanges(&m->def, d, changes, size);
    m->mod = mod;
    m->def = *d;
    m->slopeTan = d->max_slope >= 89.9f ? 1e9f : tanf(d->max_slope * 0.0174533f);
    m->axles = axles;
    return NULL;
}

/* The slot of the entity's mover among the first count, or -1. */
static LONG MvSlotOf(uint64_t entity, LONG count)
{
    for (LONG i = 0; i < count; i++) if (g_mvDefs[i].def.entity == entity) return i;
    return -1;
}

/* add_mover's answer to a redefinition, logged: 0 done, 1 refused. */
static int MvRedefined(HfMod* mod, uint64_t entity, const char* why, const char* changes)
{
    if (why) { ApiLog(mod, "add_mover(%016llx): refused: %s", (unsigned long long)entity, why); return 1; }
    ApiLog(mod, "add_mover(%016llx): redefined: %s", (unsigned long long)entity, changes);
    return 0;
}

/* SDK 35, add_mover: HellForge moves the entity's units (hellforge_move.h). Any time, best from hellforge_mod_init.
 * SDK 54: again on the game thread, it replaces the definition. 0 done, 1 refused (the log says why). */
static int ApiAddMover(HfMod* mod, const HfMoverDef* in)
{
    HfMoverDef d;
    const char* why = MvRefusal(in, &d);
    if (why) { ApiLog(mod, "add_mover: refused: %s", why); return 1; }
    /* Readers walk g_mvDefs up to the count without a lock, so a slot is written whole before the count covers it. */
    static SRWLOCK addLock = SRWLOCK_INIT;
    static const uint32_t kAxles[] = {0, 0, 0, 0, HF_AXLE_FRONT, HF_AXLE_REAR, HF_AXLE_FRONT | HF_AXLE_REAR, 0, 0, 0, 0, 0, HF_AXLE_FRONT | HF_AXLE_REAR};
    const uint32_t asked = d.mode;
    if (kAxles[d.mode]) d.mode = HF_MOVE_DRIVE; /* one driver from here on, with its axles */
    char changes[512];
    AcquireSRWLockExclusive(&addLock);
    const LONG at = g_mvDefCount, dup = MvSlotOf(d.entity, at);
    if (dup < 0 && at < MV_DEFS)
    {
        g_mvDefs[at] = (MvDef){mod, d, 0, 3.0f, d.max_slope >= 89.9f ? 1e9f : tanf(d.max_slope * 0.0174533f), kAxles[asked], asked};
        InterlockedExchange(&g_mvDefCount, at + 1);
    }
    const char* again = dup >= 0 ? MvRedefine(mod, dup, &d, asked, kAxles[asked], changes, sizeof changes) : NULL;
    ReleaseSRWLockExclusive(&addLock);
    if (dup >= 0) return MvRedefined(mod, d.entity, again, changes);
    if (at >= MV_DEFS) { ApiLog(mod, "add_mover: refused: %d movers already", MV_DEFS); return 1; }
    static const char* const kMode[] = {"", "walks", "drives", "hovers", "drives (FWD)", "drives (RWD)", "drives (4WD)", "flies (helicopter)", "flies (plane)", "sails (boat)", "sails (submarine)", "rides a rail", "drives (tracks)"}, * const kWater[] = {"", "wades", "stays dry", "floats", "sinks"};
    ApiLog(mod, "add_mover(%016llx): %s, %.1f m/s (%.1f back), %s at %.0f deg/s, brakes %.1f m/s2, %s (%.2f m)%s", (unsigned long long)d.entity,
        kMode[asked], (double)d.speed, (double)d.reverse_speed, d.turn_radius > 0 ? "turns like a vehicle" : "turns on the spot",
        (double)d.turn_rate, (double)d.brake, MvPhysical(d.mode) ? "crosses water" : kWater[d.water], (double)d.wade_depth, d.drive ? ", its own push" : "");
    if (MvPhysical(d.mode))
        ApiLog(mod, "add_mover(%016llx): holds %.2f m up, %.0f kg (%.0f%% carried by its data), %.1f Hz", (unsigned long long)d.entity,
            (double)d.hover.height, (double)d.hover.mass, (double)(d.hover.carried * 100), (double)d.hover.stiffness);
    MvStart();
    HvStart(); /* any mover with a driver's seat is driven from it */
    return 0;
}

/* SDK 35, mover_input: a driver's throttle and steer for the next half second. Game thread, on the host. 0 taken. */
static int ApiMoverInput(HfMod* mod, uint32_t entity, const HfMoverInput* in)
{
    if (!WcGameThread(mod, "mover_input") || in == NULL || in->size < HF_MOVER_INPUT_SIZE_35 || g_mvPushThread == GetCurrentThreadId()) return 1;
    HfMoverInput all;
    memset(&all, 0, sizeof all);
    memcpy(&all, in, in->size < sizeof all ? in->size : sizeof all);
    in = &all;
    AcquireSRWLockExclusive(&g_mvLock);
    MvUnit* m = MvUnitOf(entity);
    if (m != NULL && m->remote) m = NULL; /* the machine that moves it takes its inputs */
    if (m != NULL)
    {
        m->throttle = MvClamp(in->throttle, -1, 1); m->steer = MvClamp(in->steer, -1, 1); m->inputAge = 0;
        m->strafe = MvClamp(in->strafe, -1, 1); m->lift = MvClamp(in->lift, -1, 1); m->boost = MvClamp(in->boost, 0, 1);
    }
    ReleaseSRWLockExclusive(&g_mvLock);
    return m == NULL;
}

/* A unit's state as mover_state gives it. Under g_mvLock. */
static void MvSnapshot(const MvUnit* m, HfMoverState* s)
{
    memset(s, 0, sizeof *s);
    s->size = sizeof *s;
    if (m == NULL) return;
    s->status = m->started ? m->status : HF_MOVER_WAITING; s->driver = m->driver;
    memcpy(s->position, m->pos, 12);
    s->yaw = m->yaw; s->speed = m->speed; s->turning = m->turning; s->throttle = m->throttle; s->steer = m->steer;
    s->height = m->height; s->water_depth = m->waterDepth; s->grade = m->grade; s->driven = m->driven;
    s->pitch = m->pitch; s->roll = m->roll; s->boost_left = m->boostLeft; s->boosting = m->boosting; s->strafe = m->strafe; s->lift = m->lift;
}

/* SDK 35, mover_state: how a mover's unit moves (HF_MOVER_...). Any thread; an SDK 35 size gets SDK 35's fields. */
static uint32_t ApiMoverState(HfMod* mod, uint32_t entity, HfMoverState* state)
{
    (void)mod;
    HfMoverState s;
    if (g_mvPushThread == GetCurrentThreadId()) return HF_MOVER_NONE;
    AcquireSRWLockShared(&g_mvLock);
    MvSnapshot(MvUnitOf(entity), &s);
    ReleaseSRWLockShared(&g_mvLock);
    const uint32_t n = state == NULL ? 0 : state->size >= sizeof s ? (uint32_t)sizeof s : state->size >= HF_MOVER_STATE_SIZE_36 ? HF_MOVER_STATE_SIZE_36
        : state->size >= HF_MOVER_STATE_SIZE_35 ? HF_MOVER_STATE_SIZE_35 : 0;
    s.size = n;
    if (n) memcpy(state, &s, n);
    return s.status;
}

/* SDK 35, ai_drive: the custom AI drives the mover of `entity` (add_mover first). Any time. 0 done, 1 refused. */
static int ApiAiDrive(HfMod* mod, uint64_t entity, const HfAiDrive* in)
{
    if (in == NULL || in->size < offsetof(HfAiDrive, engage_range)) { ApiLog(mod, "ai_drive: refused: no HfAiDrive or its size"); return 1; }
    const char* fight = AcSet(entity, in);
    if (fight) { ApiLog(mod, "ai_drive(%016llx): refused: %s", (unsigned long long)entity, fight); return 1; }
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

#include "movers_kit.c" /* SDK 47: fixed guns and damage zones (MvUnitOf, g_hvImpulse, ActLocalAvatar) */
#include "movers_view.c" /* SDK 48: mouse_steer (TcamView, KitLocalRoot) */

/* The table's entries from SDK 35 on (ApiInit). */
#define MV_API ApiAddMover, ApiMoverInput, ApiMoverState, ApiAiDrive
