/* SDK 43: rails (add_rail, HF_MOVE_RAIL; docs/modding/movers.md "Rail vehicles"). A rail is a polyline a mod gives at
 * track height; a rail mover is placed on it by its root like a driver, one axis only: throttle moves it along the
 * line at its speed, acceleration and brake, its heading and pitch follow the track, and a line's end stops it like a
 * buffer. The game's convoy routes (research/worldgen-deep-dive.md 13) are the planned source of generated lines.
 * Included by movers.c before MvDrive. */

#define MV_RAILS 32
#define MV_RAIL_POINTS 4096
#define MV_RAIL_SNAP 20.0f /* metres: the farthest a rail mover may start from its line */

typedef struct MvRail { HfMod* mod; uint32_t count, loop; float* p; float* s; float length; } MvRail; /* s: distance at each point */
static MvRail g_mvRails[MV_RAILS];
static volatile LONG g_mvRailCount;

/* Where the line is `s` along it, and its direction there (unit length). A loop wraps; a line clamps to its ends. */
static void MvRailAt(const MvRail* r, float s, float out[3], float dir[3])
{
    s = r->loop ? fmodf(fmodf(s, r->length) + r->length, r->length) : MvClamp(s, 0, r->length);
    const uint32_t segs = r->loop ? r->count : r->count - 1;
    uint32_t lo = 0, hi = segs - 1;
    while (lo < hi) { const uint32_t mid = (lo + hi + 1) / 2; if (r->s[mid] <= s) lo = mid; else hi = mid - 1; }
    const float* a = r->p + 3 * lo, * b = r->p + 3 * ((lo + 1) % r->count);
    const float len = r->s[lo + 1] - r->s[lo], t = MvClamp((s - r->s[lo]) / len, 0, 1);
    for (int i = 0; i < 3; i++) { out[i] = a[i] + (b[i] - a[i]) * t; dir[i] = (b[i] - a[i]) / len; }
}

/* The nearest point of one rail to `p`: its distance, and how far along the rail it is. */
static float MvRailNearest(const MvRail* r, const float p[3], float* at)
{
    float best = 1e30f;
    const uint32_t segs = r->loop ? r->count : r->count - 1;
    for (uint32_t i = 0; i < segs; i++)
    {
        const float* a = r->p + 3 * i, * b = r->p + 3 * ((i + 1) % r->count);
        const float len = r->s[i + 1] - r->s[i], d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float t = MvClamp(((p[0] - a[0]) * d[0] + (p[1] - a[1]) * d[1] + (p[2] - a[2]) * d[2]) / (len * len), 0, 1);
        const float q[3] = {a[0] + d[0] * t - p[0], a[1] + d[1] * t - p[1], a[2] + d[2] * t - p[2]};
        const float dist = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
        if (dist < best) { best = dist; *at = r->s[i] + t * len; }
    }
    return best;
}

/* Puts it on its rail (def->def.rail, or the nearest), facing the way along it nearest its heading. 0 none in reach. */
static int MvRailSnap(MvUnit* m, const MvDef* def)
{
    float best = MV_RAIL_SNAP, at = 0;
    for (LONG i = 0; i < g_mvRailCount; i++)
    {
        if (g_mvRails[i].count == 0 || (def->def.rail && def->def.rail != (uint32_t)i + 1)) continue; /* 0 points: its mod was taken out */
        float s;
        const float d = MvRailNearest(&g_mvRails[i], m->pos, &s);
        if (d <= best) { best = d; at = s; m->rail = (uint32_t)i + 1; }
    }
    if (!m->rail) { if (!m->blockedLogged++) ApiLog(def->mod, "mover: unit %u: no rail within %.0f m: it can't move", m->entity, (double)MV_RAIL_SNAP); return 0; }
    float pos[3], dir[3];
    MvRailAt(&g_mvRails[m->rail - 1], at, pos, dir);
    m->railS = at;
    m->railDir = -sinf(m->yaw) * dir[0] + cosf(m->yaw) * dir[1] >= 0 ? 1.0f : -1.0f;
    ApiLog(def->mod, "mover: unit %u on rail %u, %.1f m along it (%.1f m off)", m->entity, m->rail, (double)at, (double)best);
    return 1;
}

/* One update along its rail: speed from the throttle, the track's position, heading and grade; a line's end stops it. */
static void MvRailDrive(MvUnit* m, const MvDef* def, float dt)
{
    const HfMoverDef* d = &def->def;
    if (!m->rail && !MvRailSnap(m, def)) { m->status = HF_MOVER_BLOCKED; m->speed = 0; return; }
    const MvRail* r = &g_mvRails[m->rail - 1];
    MvSpeedTurn(m, def, MvTop(m, d, dt), dt);
    m->turning = 0;
    m->driven = fabsf(m->throttle) > 0.02f ? def->axles : 0;
    float s = m->railS + m->speed * dt * m->railDir, pos[3], dir[3];
    m->status = HF_MOVER_MOVING;
    if (!r->loop && (s < 0 || s > r->length)) { s = MvClamp(s, 0, r->length); MvBlocked(m, def); }
    m->railS = r->loop ? fmodf(fmodf(s, r->length) + r->length, r->length) : s;
    MvRailAt(r, m->railS, pos, dir);
    memcpy(m->pos, pos, 12);
    const float h = sqrtf(dir[0] * dir[0] + dir[1] * dir[1]) + 1e-6f, lim = d->tilt_limit * 0.0174533f;
    m->yaw = atan2f(-dir[0] * m->railDir, dir[1] * m->railDir);
    m->grade = dir[2] / h * m->railDir;
    m->pitch = MvClamp(atanf(m->grade), -lim, lim);
    m->roll = 0;
}

/* SDK 43, add_rail: a line rail movers ride. Any time. Its id, or 0 refused. */
static uint32_t ApiAddRail(HfMod* mod, const HfRailDef* in)
{
    if (in == NULL || in->size < sizeof *in || in->points == NULL || in->count < 2 || in->count > MV_RAIL_POINTS)
    { ApiLog(mod, "add_rail: refused: no HfRailDef, its size, or 2 .. %d points", MV_RAIL_POINTS); return 0; }
    const uint32_t n = in->count, segs = in->loop ? n : n - 1;
    float* p = (float*)malloc(sizeof(float) * 3 * n), * s = (float*)malloc(sizeof(float) * (n + 1));
    if (p == NULL || s == NULL) { free(p); free(s); ApiLog(mod, "add_rail: refused: no memory"); return 0; }
    memcpy(p, in->points, sizeof(float) * 3 * n);
    s[0] = 0;
    for (uint32_t i = 0; i < segs; i++)
    {
        const float* a = p + 3 * i, * b = p + 3 * ((i + 1) % n);
        const float len = sqrtf((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]) + (b[2] - a[2]) * (b[2] - a[2]));
        if (!isfinite(len) || len < 0.05f) { free(p); free(s); ApiLog(mod, "add_rail: refused: point %u is under 5 cm from the one before, or not a number", i + 1); return 0; }
        s[i + 1] = s[i] + len;
    }
    if (MvInDrive(mod, "add_rail")) { free(p); free(s); return 0; }
    AcquireSRWLockExclusive(&g_mvLock);
    LONG at = -1;
    for (LONG i = 0; i < g_mvRailCount && at < 0; i++) if (g_mvRails[i].count == 0) at = i; /* a rail a mod taken out left */
    if (at < 0 && g_mvRailCount < MV_RAILS) at = g_mvRailCount++;
    if (at >= 0) g_mvRails[at] = (MvRail){mod, n, in->loop ? 1u : 0u, p, s, s[segs]};
    ReleaseSRWLockExclusive(&g_mvLock);
    if (at < 0) { free(p); free(s); ApiLog(mod, "add_rail: refused: %d rails already", MV_RAILS); return 0; }
    ApiLog(mod, "add_rail: rail %ld, %u points, %.0f m%s", (long)at + 1, n, (double)s[segs], in->loop ? ", a loop" : "");
    return (uint32_t)at + 1;
}

#define MV_RAIL_API ApiAddRail
