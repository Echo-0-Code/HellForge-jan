/* SDK 43: vessels (HF_MOVE_BOAT, HF_MOVE_SUB; docs/modding/movers.md "Boats and submarines"). A vessel is a physics body
 * like a hover. HellForge's buoyancy holds its root wade_depth (its draft) under the water's surface, a sub's lift
 * takes it down to dive_depth, and it is pushed and steered like a hover with a rudder (turn_radius). In water too
 * shallow for its keel it sits aground and HellForge lets go. The game draws nothing under water, so dives stay near
 * the surface. Included by movers_hover.c after movers_fly.c. */

#define BT_KEEL 0.2f /* metres of water under its keel below which it sits aground */

/* The water under it, from the surface down to the ground (30 m when the ray finds none); 0 dry. */
static float BtDepth(const MvUnit* m, const float p[3], float* surface)
{
    *surface = MvWaterAt(p[0], p[1], p[2]);
    if (*surface <= -499.0f) return 0;
    float ground = HvGroundAt(m, p[0], p[1], fmaxf(p[2], *surface) + 1.0f, -1e9f);
    /* the water's surface blocks rays as the level's own: under it, the bed is read from just below the surface */
    if (fabsf(ground - *surface) < 0.15f) ground = HvGroundAt(m, p[0], p[1], *surface - 0.1f, -1e9f);
    return ground > -1e8f ? fmaxf(*surface - ground, 0) : HV_REACH;
}

/* 1 afloat. Aground it lets go, once with a log line. */
static int BtAfloat(MvUnit* m, const MvDef* def, float depth)
{
    const int afloat = depth >= def->def.wade_depth + BT_KEEL;
    if (afloat != (int)m->engaged)
        ApiLog(def->mod, afloat ? "mover: unit %u is afloat (%.1f m of water)" : "mover: unit %u is aground (%.1f m of water): it sits", m->entity, (double)depth);
    if (afloat && !m->engaged) m->flWant = m->targetHeight = 0;
    m->engaged = (uint32_t)afloat;
    return afloat;
}

/* The depth under the surface its root heads for: its draft, plus a sub's dive (lift down, at hover.climb_rate,
 * never closer than BT_KEEL to the bottom). */
static float BtTargetZ(MvUnit* m, const HfMoverDef* d, float surface, float depth, float dt)
{
    if (d->mode == HF_MOVE_SUB)
        m->flWant = MvClamp(m->flWant - m->lift * d->hover.climb_rate * dt, 0, fmaxf(fminf(d->dive_depth, depth - d->wade_depth - BT_KEEL), 0));
    const float step = d->hover.climb_rate * dt;
    m->targetHeight = MvClamp(m->flWant, m->targetHeight - step, m->targetHeight + step);
    return surface - d->wade_depth - m->targetHeight;
}

/* One update of a vessel on the machine that simulates it. */
static void BtControl(MvUnit* m, const MvDef* def, float dt)
{
    const HfMoverDef* d = &def->def;
    float w[16], v[3] = {0}, om[3] = {0}, surface;
    if (!HvRead(m, w, v, om)) return;
    const float* p = w + 12;
    const float depth = BtDepth(m, p, &surface);
    m->waterDepth = depth;
    m->height = depth > 0 ? p[2] - surface : 0;
    if (!BtAfloat(m, def, depth)) { m->status = fabsf(m->throttle) > 0.05f ? HF_MOVER_BLOCKED : HF_MOVER_WAITING; return; }
    m->status = HF_MOVER_MOVING;
    const float wn = 6.2831853f * d->hover.stiffness, err = BtTargetZ(m, d, surface, depth, dt) - p[2], mass = d->hover.mass;
    const float az = (1.0f - d->hover.carried) * MV_G + MvClamp(wn * wn * err - 2 * d->hover.damping * wn * v[2], -0.5f * MV_G, 0.8f * MV_G);
    float fl[2] = {-sinf(m->yaw), cosf(m->yaw)}, a[2] = {0, 0}, yawRate = 0;
    HvDrive(m, def, fl, v, dt, a, &yawRate);
    const float j[3] = {a[0] * mass * dt, a[1] * mass * dt, az * mass * dt}, n[3] = {0, 0, 1}; /* water is level */
    g_hvImpulse(m->body, j, NULL);
    HvAttitude(m, d, w, om, n, yawRate, dt);
    m->speed = v[0] * fl[0] + v[1] * fl[1];
    m->turning = om[2];
}
