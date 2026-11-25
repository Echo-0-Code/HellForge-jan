/* SDK 43: fliers (HF_MOVE_HELI, HF_MOVE_PLANE; docs/modding/movers.md "Fly"). A flier is a physics body like a hover
 * and is pushed the same way, but it takes off and lands: it waits until it sits on the ground (a Pelican's drop),
 * climbs to hover.height when someone flies it, and sets down when nobody has for FL_LAND_S. A helicopter's thrust
 * tilts toward the way it speeds up; a plane's wings carry it only near stall_speed, and it banks to turn. Included by
 * movers_hover.c after HvControl. */

#define FL_LAND_S 3.0f    /* seconds with nobody flying it before it sets down */
#define FL_TILT_MAX 0.52f /* radians: the most a helicopter tilts to speed up, 30 degrees */
#define FL_PITCH_MAX 0.35f /* radians: the steepest climb or dive a plane's nose follows, 20 degrees */
#define FL_GEAR 0.5f      /* metres over height_min that still count as sitting on its gear */

/* Rays reach past its highest height, so a cruise over hills keeps reading the ground. */
static float FlReach(const HfMoverDef* d) { return fminf(d->hover.height_max * 1.5f + 30.0f, 800.0f); }

/* Its gap over the surface. It takes hold once it sits on the ground; without a reading it holds over the last one. */
static float FlGap(MvUnit* m, const MvDef* def, const float p[3], float dt)
{
    const HfMoverHover* h = &def->def.hover;
    const float reach = FlReach(&def->def), surface = HvSurface(m, p, reach);
    const int good = surface > -1e8f && p[2] - surface > -3.0f && p[2] - surface < reach;
    if (good) m->lastSurface = surface;
    const float gap = p[2] - (good || !m->engaged ? surface : m->lastSurface);
    m->height = good || m->engaged ? gap : 0;
    m->lost = good ? 0 : m->lost + dt;
    if (!m->engaged && good && gap < h->height_min + 1.0f)
    {
        m->engaged = 1;
        m->targetHeight = m->flWant = h->height_min;
        m->flDatum = surface;
        if (def->def.mode == HF_MOVE_HELI) m->flWant = -2.0f; /* collective down: it sits */
        ApiLog(def->mod, "mover: unit %u is on the ground (%.2f m): it flies when driven", m->entity, (double)gap);
    }
    return gap;
}

/* The height it heads for: hover.height once anyone flies it, moved by lift, height_min (landing) once nobody has for
 * FL_LAND_S, and never above its gear while it can't climb (a plane under stall_speed). It moves at climb_rate. */
static void FlTarget(MvUnit* m, const HfMoverHover* h, float dt, int canClimb)
{
    const int asked = fabsf(m->throttle) > 0.05f || fabsf(m->steer) > 0.05f || fabsf(m->strafe) > 0.05f || m->lift > 0.05f;
    m->flIdle = m->driver != HF_DRIVER_NONE ? 0 : m->flIdle + dt;
    if (m->driver != HF_DRIVER_NONE && asked && m->flWant < h->height) m->flWant = h->height;
    if (m->flIdle > FL_LAND_S) m->flWant = h->height_min;
    m->flWant = MvClamp(m->flWant + m->lift * h->climb_rate * dt, h->height_min, h->height_max);
    const float want = canClimb ? m->flWant : h->height_min, step = h->climb_rate * dt;
    m->targetHeight = MvClamp(want, m->targetHeight - 1.5f * step, m->targetHeight + step);
}

/* Upward m/s2: the weight its data leaves plus a spring-damper toward targetHeight, no faster than climb_rate up or
 * 1.5 x down, all of it times `authority` (a plane's wing lift). */
static float FlHeave(const MvUnit* m, const HfMoverHover* h, float gap, float vz, float authority)
{
    const float wn = 6.2831853f * h->stiffness, climbMax = 1.2f * h->climb_rate, dropMax = 1.5f * h->climb_rate + 1.0f;
    float lift = wn * wn * (m->targetHeight - gap) - 2 * h->damping * wn * vz;
    if (vz > climbMax) lift = fminf(lift, -2.0f * (vz - climbMax));
    if (vz < -dropMax) lift = fmaxf(lift, 2.0f * (-dropMax - vz));
    return ((1.0f - h->carried) * MV_G + MvClamp(lift, -0.5f * MV_G, 0.8f * MV_G)) * authority;
}

/* A helicopter's collective (Jan's design): the keys move the altitude where its thrust equals its weight, and it stays
 * there when they let go. Thrust falls with air density, exp(-alt / H). Real air's H is 8.4 km; H = height_max / ln
 * thrust_max puts its ceiling at height_max over flDatum. No ground effect: at this H its real 10 % would hold it 15 m
 * up. A broken engine or tail zone (znSink) lowers the collective and holds the fall to znSink, as autorotation does. */
static float FlHeliHeave(MvUnit* m, const HfMoverDef* d, const float p[3], float gap, float vz, float dt)
{
    const HfMoverHover* h = &d->hover;
    const float scale = h->height_max / logf(d->thrust_max), alt = p[2] - m->flDatum, groundAlt = alt - gap;
    const float wn = sqrtf(MV_G / scale); /* thinning air acts as a spring of g / H, damped by damping x critical */
    m->flIdle = m->driver != HF_DRIVER_NONE ? 0 : m->flIdle + dt;
    if (m->znSink > 0) m->flWant = fminf(m->flWant, alt) - m->znSink * dt; /* it can only come down */
    else if (m->flIdle > FL_LAND_S) m->flWant -= 0.5f * h->climb_rate * dt; /* nobody flies it: it lowers the collective */
    else m->flWant += m->lift * h->climb_rate * dt;
    m->flWant = MvClamp(m->flWant, groundAlt - 2.0f, h->height_max); /* 2 m under the ground: it sits on its gear */
    m->targetHeight = m->flWant - groundAlt;
    const float thrust = fminf(expf((m->flWant - alt) / scale), d->thrust_max); /* times its weight */
    float az = (1.0f - h->carried) * MV_G * thrust - 2 * h->damping * wn * vz;
    if (m->znSink > 0 && vz < -m->znSink) az = fmaxf(az, (1.0f - h->carried) * MV_G + 2.0f * (-m->znSink - vz));
    return az;
}

/* A plane's horizontal push: thrust up to speed against quadratic drag, throttle below 0 brakes (air brakes aloft,
 * wheel brakes rolling, reverse_speed taxiing), parking brake on its gear, and sideslip damped by side_drag. Aloft its
 * steer banks it and the bank sets the turn, g tan(bank) / speed; on its gear it steers like a car. */
static void FlPlaneDrive(MvUnit* m, const MvDef* def, const float fl[2], const float v[3], float dt, float a[2], float* yawRate)
{
    const HfMoverDef* d = &def->def;
    const int onGround = m->height < d->hover.height_min + FL_GEAR;
    const float vf = v[0] * fl[0] + v[1] * fl[1], side = -fl[1] * v[0] + fl[0] * v[1], vh = sqrtf(v[0] * v[0] + v[1] * v[1]);
    const float t = m->throttle, top = MvTop(m, d, dt);
    float push = t > 0 ? t * d->acceleration * (top * top) / (d->speed * d->speed) : 0;
    if (t < -0.05f && vf > 0.1f) push = fmaxf(t * d->brake, -vf / dt);
    else if (t < -0.05f && onGround) push = t * d->acceleration * (d->reverse_speed * d->reverse_speed) / (d->speed * d->speed);
    else if (onGround && fabsf(t) < 0.05f && fabsf(vf) < 1.5f) push = -MvClamp(vf / dt, -d->brake, d->brake);
    *yawRate = onGround || vf < d->stall_speed ? m->steer * MvRate(d, vf) : MV_G * tanf(m->steer * d->bank_max * 0.0174533f) / vf;
    if (d->drive != NULL) { const HfMoverPush p = MvPush(m, def, dt); push = p.forward; *yawRate = p.turn; }
    const float cd = d->acceleration / (d->speed * d->speed);
    a[0] = push * fl[0] - cd * vh * v[0] + d->hover.side_drag * side * fl[1];
    a[1] = push * fl[1] - cd * vh * v[1] - d->hover.side_drag * side * fl[0];
}

/* The up it rights itself to: a helicopter's thrust along the push it wants plus its weight, at most FL_TILT_MAX
 * (or tilt_limit) off vertical; a plane's banked into the turn and pitched along its climb. */
static void FlUp(const HfMoverDef* d, const float fl[2], const float v[3], const float a[2], float yawRate, float n[3])
{
    if (d->mode == HF_MOVE_PLANE)
    {
        const float vf = fmaxf(v[0] * fl[0] + v[1] * fl[1], d->stall_speed), lat = vf * yawRate;
        const float climb = tanf(MvClamp(atan2f(v[2], vf), -FL_PITCH_MAX, FL_PITCH_MAX)) * MV_G;
        n[0] = -fl[1] * lat - fl[0] * climb; n[1] = fl[0] * lat - fl[1] * climb; n[2] = MV_G;
    }
    else
    {
        const float lim = tanf(d->tilt_limit > 0 ? fminf(d->tilt_limit * 0.0174533f, 1.2f) : FL_TILT_MAX) * MV_G, ah = sqrtf(a[0] * a[0] + a[1] * a[1]);
        const float k = ah > lim ? lim / ah : 1.0f;
        n[0] = a[0] * k; n[1] = a[1] * k; n[2] = MV_G;
    }
    const float l = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    n[0] /= l; n[1] /= l; n[2] /= l;
}

/* One update of a flier on the machine that simulates it. */
static void FlControl(MvUnit* m, const MvDef* def, float dt)
{
    const HfMoverDef* d = &def->def;
    float w[16], v[3] = {0}, om[3] = {0};
    if (!HvRead(m, w, v, om)) return;
    const float gap = FlGap(m, def, w + 12, dt), mass = d->hover.mass;
    if (!m->engaged) { m->status = HF_MOVER_WAITING; return; }
    m->status = HF_MOVER_MOVING;
    float fl[2] = {-sinf(m->yaw), cosf(m->yaw)}, a[2] = {0, 0}, yawRate = 0, n[3];
    const int plane = d->mode == HF_MOVE_PLANE, onGround = gap < d->hover.height_min + FL_GEAR;
    const float vf = v[0] * fl[0] + v[1] * fl[1], wing = plane ? MvClamp(vf * vf / (d->stall_speed * d->stall_speed), 0, 1) : 1.0f;
    float az;
    if (plane && m->znSink > 0) m->lift = -1.0f; /* a broken engine or control zone: it glides down */
    if (plane)
    {
        FlTarget(m, &d->hover, dt, vf >= d->stall_speed);
        az = FlHeave(m, &d->hover, gap, v[2], onGround ? 1.0f : wing);
        FlPlaneDrive(m, def, fl, v, dt, a, &yawRate);
    }
    else { az = FlHeliHeave(m, d, w + 12, gap, v[2], dt); HvDrive(m, def, fl, v, dt, a, &yawRate); }
    yawRate += m->znSpin; /* a broken tail rotor's torque turns it whatever the pedals do */
    const float j[3] = {a[0] * mass * dt, a[1] * mass * dt, az * mass * dt};
    g_hvImpulse(m->body, j, NULL);
    if (onGround && m->targetHeight <= (plane ? d->hover.height_min + 0.05f : FL_GEAR)) HvTargetUp(m, d, w, n);
    else FlUp(d, fl, v, a, yawRate, n);
    HvAttitude(m, d, w, om, n, yawRate, dt);
    m->speed = vf;
    m->turning = om[2];
}
