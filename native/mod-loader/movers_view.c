/* SDK 48: mouse_steer (docs/modding/movers.md "Mouse steering"). A mover with HfMoverDef.mouse_steer 1, driven from its
 * seat by this machine's Helldiver, turns and climbs toward where this machine's camera looks (terminal_camera.c reads
 * the world's last render pose). A/D steer by hand while held (a plane's roll), and W/S move a throttle lever that stays
 * where it is left, so a pilot can fly with the mouse alone. Another machine's driver has no camera here: that seat
 * drives with the keys as before. Included by movers.c after movers_kit.c (KitLocalRoot). */

#define VW_SEAT_R 1.6f       /* metres from the unit's driver seat joint that count as sitting in it */
#define VW_LEVER_RATE 0.6f   /* the throttle lever's travel a second with W or S held */
#define VW_TURN_GAIN 1.6f    /* a plane's turn rate, rad/s, per radian between its heading and the view's */
#define VW_TURN_MAX 1.2f     /* the fastest turn the view asks of a plane, rad/s (bank_max caps it anyway) */
#define VW_STEER_SPAN 0.35f  /* on the ground (or not a plane): radians off the view's heading for full steer */
#define VW_DEAD 0.05f        /* radians of view pitch that ask for no climb (about 3 degrees) */
#define VW_HELI_SPAN 0.35f   /* a helicopter: view pitch, radians, for full lift */

static uint32_t g_vwDriverNode;

/* Whether this machine's Helldiver sits in the unit's driver seat (its attach_driver joint; else near its root). */
static int ViewLocalDriver(const MvUnit* m)
{
    float root[3], w[16], at[3];
    if (!KitLocalRoot(root)) return 0;
    if (g_vwDriverNode == 0) g_vwDriverNode = (uint32_t)(ApiResourceHash("attach_driver") >> 32);
    if (ApiJointPose(&g_mvMod, m->unit, g_vwDriverNode, HF_POSE_WORLD, w)) { at[0] = w[12]; at[1] = w[13]; at[2] = w[14]; }
    else { at[0] = m->pos[0]; at[1] = m->pos[1]; at[2] = m->pos[2] + 1.0f; }
    const float dx = root[0] - at[0], dy = root[1] - at[1], dz = root[2] - at[2];
    return dx * dx + dy * dy + dz * dz < VW_SEAT_R * VW_SEAT_R;
}

/* HvSeat's input, for a mouse_steer mover this machine's Helldiver drives: W/S move the throttle lever (S with the
 * lever closed brakes, and reverses on the ground as the game's lever does), A/D are kept apart for ViewSteer. Returns 0
 * when the seat drives as before. Under g_mvLock. */
static int ViewSeat(MvUnit* m, const float in[3], uint32_t lever, float dt)
{
    const MvDef* def = m->def ? &g_mvDefs[m->def - 1] : NULL;
    m->seatLocal = def != NULL && def->def.mouse_steer && ViewLocalDriver(m);
    if (!m->seatLocal) { m->seatLever = 0; return 0; }
    const float keys = lever == 0 ? -fmaxf(in[1], in[2]) : in[1] - in[2];
    float throttle;
    if (keys > 0.02f) m->seatLever = fminf(1.0f, m->seatLever + keys * VW_LEVER_RATE * dt);
    else if (keys < -0.02f && m->seatLever > 0) m->seatLever = fmaxf(0.0f, m->seatLever + keys * VW_LEVER_RATE * dt);
    throttle = keys < -0.02f && m->seatLever <= 0 ? keys : m->seatLever;
    m->seatThrottle = MvClamp(throttle, -1, 1);
    m->seatKeys = MvClamp(-in[0], -1, 1);
    m->seatSteer = m->seatKeys;
    m->seatLift = HvLiftKeys();
    /* it counts as driven while the lever is open or anything is pressed; the view alone moves it once it is */
    if (m->seatLever > 0.02f || fabsf(keys) > 0.02f || fabsf(in[0]) > 0.02f || m->seatLift != 0) m->seatAge = 0;
    return 1;
}

/* The steer that turns it onto the view's heading, e radians to its left: a plane aloft banks for a turn rate that
 * grows with e (g tan(bank) / speed, as movers_fly.c turns it); anything else steers like wheels. */
static float ViewTurn(const MvUnit* m, const HfMoverDef* d, float e)
{
    const int aloft = d->mode == HF_MOVE_PLANE && m->speed >= d->stall_speed && m->height > d->hover.height_min + 0.5f;
    if (!aloft) return MvClamp(e / VW_STEER_SPAN, -1, 1);
    const float rate = MvClamp(VW_TURN_GAIN * e, -VW_TURN_MAX, VW_TURN_MAX);
    return MvClamp(atanf(rate * m->speed / MV_G) / (d->bank_max * 0.0174533f), -1, 1);
}

/* A flier's lift for the view's pitch: a plane climbs along it, a helicopter moves its collective. */
static float ViewLift(const MvUnit* m, const HfMoverDef* d, float pitch)
{
    pitch = fabsf(pitch) < VW_DEAD ? 0 : pitch - (pitch > 0 ? VW_DEAD : -VW_DEAD);
    if (d->mode != HF_MOVE_PLANE) return MvClamp(pitch / VW_HELI_SPAN, -1, 1);
    return m->speed > 1.0f ? MvClamp(m->speed * sinf(pitch) / d->hover.climb_rate, -1, 1) : 0;
}

/* MvDriver, a seated mouse_steer driver: steer (and a flier's lift) toward the view, unless A/D (or Space/Control)
 * are held. */
static void ViewSteer(MvUnit* m, const MvDef* def)
{
    const HfMoverDef* d = &def->def;
    float v[16];
    if (!TcamView(v)) return;
    const float* f = v + 4;
    const float h = sqrtf(f[0] * f[0] + f[1] * f[1]);
    if (h < 1e-3f) return;
    const float e = remainderf(atan2f(-f[0], f[1]) - m->yaw, 6.28318531f); /* yaw: counter-clockwise from +Y */
    if (fabsf(m->seatKeys) < 0.1f) m->steer = ViewTurn(m, d, e);
    if ((d->mode == HF_MOVE_PLANE || d->mode == HF_MOVE_HELI) && m->seatLift == 0) m->lift = ViewLift(m, d, atan2f(f[2], h));
}
