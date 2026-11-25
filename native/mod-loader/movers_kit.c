/* SDK 47: a flier's kit (docs/modding/movers.md "Fixed weapons" and "Damage zones"): fixed forward guns its driver
 * fires, and damage zones whose breaking costs it power or control. Included by movers.c after movers_hover.c
 * (HvLiftKeys's window check, the physics calls) and before the API table.
 *
 * The game has no seat flag we read, so this machine's Helldiver drives while its root is within KIT_SEAT_R of the
 * gun's `seat`, as the quad's rider fit tests. Zones run before HF_ON_DAMAGE_PRE's listeners (event_sources.c), so a
 * zone's scale is what the mods see; a broken engine or control zone sets m->znSink, which movers_fly.c obeys. */

#define KIT_GUNS 64
#define KIT_ZONES 64
#define KIT_SEAT_R 1.2f   /* metres: a Helldiver's root this near the seat sits in it */
#define KIT_MAX_SHOTS 4   /* rounds one update may fire (a 60 fps update at 3,000 rpm fires about 1) */

typedef struct KitGun { HfMod* mod; HfFixedGunDef def; uint32_t slot; } KitGun;   /* slot: its index on its mover */
typedef struct KitZone { HfMod* mod; HfDamageZoneDef def; uint32_t slot; } KitZone;

static KitGun g_kitGuns[KIT_GUNS];
static volatile LONG g_kitGunCount;
static KitZone g_kitZones[KIT_ZONES];
static volatile LONG g_kitZoneCount;
static uint32_t g_kitRandom = 0x9E3779B9u;

static float KitRandom(void) { g_kitRandom ^= g_kitRandom << 13; g_kitRandom ^= g_kitRandom >> 17; g_kitRandom ^= g_kitRandom << 5; return (g_kitRandom & 0xFFFFFF) / 16777216.0f; }

/* A frame vector (x right, y forward, z up) in the world, from its root pose w; with `at`, a point. */
static void KitToWorld(const float w[16], const float v[3], int at, float out[3])
{
    for (int k = 0; k < 3; k++) out[k] = w[k] * v[0] + w[4 + k] * v[1] + w[8 + k] * v[2] + (at ? w[12 + k] : 0);
}

/* A world point in the frame of the root pose w (its axis rows normalised: the pose may carry scale). */
static void KitToLocal(const float w[16], const float p[3], float out[3])
{
    const float d[3] = {p[0] - w[12], p[1] - w[13], p[2] - w[14]};
    for (int r = 0; r < 3; r++)
    {
        const float* a = w + 4 * r;
        const float l = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
        out[r] = l > 1e-6f ? (a[0] * d[0] + a[1] * d[1] + a[2] * d[2]) / l : 0;
    }
}

static uint32_t KitDefOf(uint64_t entity)
{
    for (LONG d = 0; d < g_mvDefCount; d++) if (g_mvDefs[d].def.entity == entity) return (uint32_t)d + 1;
    return 0;
}

/* The first update of a unit with a kit: its guns full and its zones whole. */
static void KitInit(MvUnit* m)
{
    if (m->kitInit) return;
    m->kitInit = 1;
    const uint64_t resource = g_mvDefs[m->def - 1].def.entity;
    for (LONG i = 0; i < g_kitGunCount; i++) if (g_kitGuns[i].def.entity == resource) m->gunRounds[g_kitGuns[i].slot] = g_kitGuns[i].def.rounds;
    for (LONG i = 0; i < g_kitZoneCount; i++) if (g_kitZones[i].def.entity == resource) m->znHealth[g_kitZones[i].slot] = g_kitZones[i].def.health;
}

/* The trigger's button, held while the game's window has the keyboard (HvLiftKeys's check). */
static int KitTrigger(uint32_t trigger)
{
    DWORD process = 0;
    const HWND w = GetForegroundWindow();
    if (w == NULL || !GetWindowThreadProcessId(w, &process) || process != GetCurrentProcessId()) return 0;
    return (GetAsyncKeyState(trigger == HF_TRIGGER_ALT ? VK_RBUTTON : VK_LBUTTON) & 0x8000) != 0;
}

/* This machine's Helldiver's root, or 0. */
static int KitLocalRoot(float out[3])
{
    const uintptr_t game = (uintptr_t)GetModuleHandleW(L"game.dll");
    ActHealth h;
    const char* why;
    if (!game || !ActLocalAvatar(game, 0, &h, &why)) return 0;
    return EvpUnitPosition(h.unit, out);
}

/* Whether this machine's Helldiver sits in the gun's seat. */
static int KitSeated(const HfFixedGunDef* d, const float w[16])
{
    float root[3], seat[3];
    if (!KitLocalRoot(root)) return 0;
    KitToWorld(w, d->seat, 1, seat);
    const float dx = root[0] - seat[0], dy = root[1] - seat[1], dz = root[2] - seat[2];
    return dx * dx + dy * dy + dz * dz < KIT_SEAT_R * KIT_SEAT_R;
}

/* One round along the nose, scattered and converged; 0 when spawn_projectile refused it (its log says why). */
static int KitShot(MvUnit* m, const KitGun* g, const float w[16], float period)
{
    const HfFixedGunDef* d = &g->def;
    float dir[3] = {d->converge > 0 ? -d->muzzle[0] / d->converge : 0, 1, 0}, at[3], wd[3];
    if (d->spread > 0)
    {
        const float r = tanf(d->spread * 0.5f * 0.0174533f) * sqrtf(KitRandom()), a = 6.2831853f * KitRandom();
        dir[0] += r * cosf(a); dir[2] += r * sinf(a);
    }
    KitToWorld(w, d->muzzle, 1, at);
    KitToWorld(w, dir, 0, wd);
    if (ApiSpawnProjectile(g->mod, d->projectile, at, wd) == 0) return 0;
    if (d->recoil > 0 && m->body)
    {
        float back[3];
        const float b[3] = {0, -d->recoil * period, 0};
        KitToWorld(w, b, 0, back);
        g_hvImpulse(m->body, back, at);
    }
    return 1;
}

static void KitFire(MvUnit* m, const KitGun* g, const float w[16], float dt)
{
    const HfFixedGunDef* d = &g->def;
    const uint32_t s = g->slot;
    m->gunWait[s] = fmaxf(m->gunWait[s] - dt, -dt);
    m->gunSeated[s] = KitSeated(d, w);
    const int held = m->gunSeated[s] && KitTrigger(d->trigger);
    if (held && !m->gunHeld[s]) m->gunBurst[s] = d->burst ? d->burst : 0xFFFFFFFFu; /* a new press */
    m->gunHeld[s] = (uint8_t)held;
    m->gunFiring[s] = held && m->gunBurst[s] > 0 && m->gunRounds[s] > 0;
    const float period = 60.0f / d->fire_rate;
    for (int shots = 0; m->gunFiring[s] && shots < KIT_MAX_SHOTS && m->gunWait[s] <= 0 && m->gunRounds[s] > 0 && m->gunBurst[s] > 0; shots++)
    {
        if (!KitShot(m, g, w, period)) { m->gunWait[s] = 1.0f; return; }
        m->gunRounds[s]--;
        if (m->gunBurst[s] != 0xFFFFFFFFu) m->gunBurst[s]--;
        m->gunWait[s] += period;
        if (m->gunRounds[s] == 0) ApiLog(g->mod, "fixed gun %u on unit %u: empty", s + 1, m->entity);
    }
}

/* Each update, under g_mvLock: the unit's guns on the host. */
static void KitTick(MvUnit* m, float dt)
{
    if (m->remote || !m->unit || (g_kitGunCount == 0 && g_kitZoneCount == 0)) return;
    KitInit(m);
    if (g_kitGunCount == 0) return;
    float w[16];
    if (!ApiJointPose(&g_mvMod, m->unit, 0, HF_POSE_WORLD, w) || !isfinite(w[12])) return;
    const uint64_t resource = g_mvDefs[m->def - 1].def.entity;
    for (LONG i = 0; i < g_kitGunCount; i++) if (g_kitGuns[i].def.entity == resource) KitFire(m, &g_kitGuns[i], w, dt);
}

/* A zone broke: its effect on the unit (the slowest sink of all broken zones, their spins added). */
static void ZnBreak(MvUnit* m, const KitZone* z)
{
    if (z->def.effect != HF_ZONE_PLAIN) m->znSink = m->znSink > 0 ? fminf(m->znSink, z->def.sink) : z->def.sink;
    if (z->def.effect == HF_ZONE_CONTROL) m->znSpin += z->def.spin * 0.0174533f;
}

/* The first zone of the unit a hit at `local` lands in, or NULL. */
static const KitZone* ZnAt(const MvUnit* m, const float local[3])
{
    const uint64_t resource = g_mvDefs[m->def - 1].def.entity;
    for (LONG i = 0; i < g_kitZoneCount; i++)
    {
        const KitZone* z = &g_kitZones[i];
        const float dx = local[0] - z->def.center[0], dy = local[1] - z->def.center[1], dz = local[2] - z->def.center[2];
        if (z->def.entity == resource && dx * dx + dy * dy + dz * dz <= z->def.radius * z->def.radius) return z;
    }
    return NULL;
}

/* Before the mods' damage listeners (ConsumeDamageDetour), on the game thread: hits landing in a zone. */
static void ZnDamage(uint8_t* event)
{
    if (g_kitZoneCount == 0 || !TryAcquireSRWLockExclusive(&g_mvLock)) return;
    uint32_t target;
    int32_t amount;
    float p[3], w[16], local[3];
    memcpy(&target, event, 4); memcpy(&amount, event + 0x2C, 4); memcpy(p, event + 0x50, 12);
    MvUnit* m = MvUnitOf(target);
    const KitZone* z = NULL;
    if (m != NULL && !m->remote && m->unit && amount > 0 && ApiJointPose(&g_mvMod, m->unit, 0, HF_POSE_WORLD, w))
    {
        KitInit(m);
        KitToLocal(w, p, local);
        z = ZnAt(m, local);
    }
    if (z != NULL)
    {
        int32_t* hp = &m->znHealth[z->slot];
        if (*hp > 0 && *hp <= amount)
        {
            ZnBreak(m, z);
            static const char* const kEffect[] = {"", ": it comes down", ": it spins and comes down"};
            ApiLog(z->mod, "damage zone %u on unit %u broke%s", z->slot + 1, m->entity, kEffect[z->def.effect]);
        }
        *hp = *hp > amount ? *hp - amount : 0;
        const int32_t scaled = z->def.scale < 0 ? 0 : (int32_t)lroundf((float)amount * z->def.scale);
        memcpy(event + 0x2C, &scaled, 4);
    }
    ReleaseSRWLockExclusive(&g_mvLock);
}

static int ZnWantsDamage(void) { return g_kitZoneCount > 0; }

/* Its slot on its mover (one past the last), or MV_KIT_SLOTS when that mover is full. */
static uint32_t KitSlot(uint64_t entity, int zones)
{
    uint32_t slot = 0;
    const LONG count = zones ? g_kitZoneCount : g_kitGunCount;
    for (LONG i = 0; i < count; i++) if ((zones ? g_kitZones[i].def.entity : g_kitGuns[i].def.entity) == entity) slot++;
    return slot < MV_KIT_SLOTS ? slot : MV_KIT_SLOTS;
}

static const char* KitGunRefusal(const HfFixedGunDef* in, HfFixedGunDef* d)
{
    if (in == NULL || in->size < sizeof(HfFixedGunDef) || in->entity == 0) return "no HfFixedGunDef, its size or its entity";
    *d = *in;
    if (!d->trigger) d->trigger = HF_TRIGGER_FIRE;
    if (!d->rounds) d->rounds = 100;
    if (d->fire_rate == 0) d->fire_rate = 600;
    if (!KitDefOf(d->entity)) return "add_mover it first";
    if (d->trigger > HF_TRIGGER_ALT || d->projectile == 0 || !AiIn(d->fire_rate, 1, 6000) || d->rounds > 100000 || !AiIn(d->spread, 0, 30) ||
        !AiIn(d->converge, 0, 2000) || !AiIn(d->recoil, 0, 1e6f))
        return "trigger FIRE or ALT, a projectile, fire_rate 1 .. 6000, rounds up to 100000, spread 0 .. 30, converge 0 .. 2000, recoil 0 .. 1e6";
    for (int k = 0; k < 3; k++)
        if (!AiIn(d->muzzle[k], -50, 50) || !AiIn(d->seat[k], -50, 50)) return "muzzle and seat within 50 m of its root";
    return KitSlot(d->entity, 0) >= MV_KIT_SLOTS ? "8 guns on that mover already" : NULL;
}

/* SDK 47, add_fixed_gun: the gun's id (1 and up), or 0 refused (the log says why). */
static uint32_t ApiAddFixedGun(HfMod* mod, const HfFixedGunDef* in)
{
    HfFixedGunDef d;
    const char* why = KitGunRefusal(in, &d);
    if (why) { ApiLog(mod, "add_fixed_gun: refused: %s", why); return 0; }
    const uint32_t slot = KitSlot(d.entity, 0);
    const LONG at = InterlockedIncrement(&g_kitGunCount) - 1;
    if (at >= KIT_GUNS) { InterlockedDecrement(&g_kitGunCount); ApiLog(mod, "add_fixed_gun: refused: %d guns already", KIT_GUNS); return 0; }
    g_kitGuns[at] = (KitGun){mod, d, slot};
    ApiLog(mod, "add_fixed_gun(%016llx): gun %u, projectile %u, %.0f rpm, %u rounds, %s, at %.2f %.2f %.2f", (unsigned long long)d.entity, slot + 1,
        d.projectile, (double)d.fire_rate, d.rounds, d.trigger == HF_TRIGGER_ALT ? "aim button" : "fire button", (double)d.muzzle[0], (double)d.muzzle[1], (double)d.muzzle[2]);
    return slot + 1;
}

static uint32_t ApiFixedGunState(HfMod* mod, uint32_t entity, uint32_t gun, HfFixedGunState* out)
{
    (void)mod;
    if (out == NULL || out->size < sizeof *out || gun == 0 || gun > MV_KIT_SLOTS) return 0;
    uint32_t found = 0;
    AcquireSRWLockShared(&g_mvLock);
    const MvUnit* m = MvUnitOf(entity);
    if (m != NULL)
        for (LONG i = 0; i < g_kitGunCount; i++)
            if (g_kitGuns[i].def.entity == g_mvDefs[m->def - 1].def.entity && g_kitGuns[i].slot == gun - 1)
            {
                const uint32_t s = gun - 1;
                *out = (HfFixedGunState){sizeof *out, m->kitInit ? m->gunRounds[s] : g_kitGuns[i].def.rounds, g_kitGuns[i].def.rounds, m->gunFiring[s], m->gunSeated[s]};
                found = 1;
            }
    ReleaseSRWLockShared(&g_mvLock);
    return found;
}

static int ApiFixedGunRefill(HfMod* mod, uint32_t entity, uint32_t gun, uint32_t rounds)
{
    int done = 1;
    AcquireSRWLockExclusive(&g_mvLock);
    MvUnit* m = MvUnitOf(entity);
    if (m != NULL)
    {
        KitInit(m);
        for (LONG i = 0; i < g_kitGunCount; i++)
        {
            const KitGun* g = &g_kitGuns[i];
            if (g->def.entity != g_mvDefs[m->def - 1].def.entity || (gun && g->slot != gun - 1)) continue;
            const uint32_t room = g->def.rounds - m->gunRounds[g->slot];
            m->gunRounds[g->slot] += rounds == 0 || rounds > room ? room : rounds;
            done = 0;
        }
    }
    ReleaseSRWLockExclusive(&g_mvLock);
    if (done) ApiLog(mod, "fixed_gun_refill: unit %u has no such gun", entity);
    return done;
}

static const char* ZnRefusal(const HfDamageZoneDef* in, HfDamageZoneDef* d)
{
    if (in == NULL || in->size < sizeof(HfDamageZoneDef) || in->entity == 0) return "no HfDamageZoneDef, its size or its entity";
    *d = *in;
    if (d->radius == 0) d->radius = 0.5f;
    if (d->health == 0) d->health = 500;
    if (d->scale == 0) d->scale = 1;
    if (d->sink == 0) d->sink = 5;
    if (d->spin == 0) d->spin = 60;
    if (!KitDefOf(d->entity)) return "add_mover it first";
    if (d->effect > HF_ZONE_CONTROL || !AiIn(d->radius, 0.05f, 20) || d->health < 1 || d->health > 1000000 || !AiIn(d->scale, -1, 10) ||
        !AiIn(d->sink, 0.5f, 30) || !AiIn(d->spin, -720, 720))
        return "effect PLAIN, ENGINE or CONTROL, radius 0.05 .. 20, health 1 .. 1000000, scale -1 .. 10, sink 0.5 .. 30, spin -720 .. 720";
    return KitSlot(d->entity, 1) >= MV_KIT_SLOTS ? "8 zones on that mover already" : NULL;
}

/* SDK 47, add_damage_zone: the zone's id (1 and up), or 0 refused (the log says why). */
static uint32_t ApiAddDamageZone(HfMod* mod, const HfDamageZoneDef* in)
{
    HfDamageZoneDef d;
    const char* why = ZnRefusal(in, &d);
    if (why) { ApiLog(mod, "add_damage_zone: refused: %s", why); return 0; }
    const uint32_t slot = KitSlot(d.entity, 1);
    const LONG at = InterlockedIncrement(&g_kitZoneCount) - 1;
    if (at >= KIT_ZONES) { InterlockedDecrement(&g_kitZoneCount); ApiLog(mod, "add_damage_zone: refused: %d zones already", KIT_ZONES); return 0; }
    g_kitZones[at] = (KitZone){mod, d, slot};
    static const char* const kEffect[] = {"plain", "engine", "control"};
    ApiLog(mod, "add_damage_zone(%016llx): zone %u, %s, %.2f m at %.2f %.2f %.2f, %d health, x%.2f to the vehicle", (unsigned long long)d.entity, slot + 1,
        kEffect[d.effect], (double)d.radius, (double)d.center[0], (double)d.center[1], (double)d.center[2], d.health, (double)(d.scale < 0 ? 0 : d.scale));
    return slot + 1;
}

static uint32_t ApiZoneState(HfMod* mod, uint32_t entity, uint32_t zone, HfZoneState* out)
{
    (void)mod;
    if (out == NULL || out->size < sizeof *out || zone == 0 || zone > MV_KIT_SLOTS) return 0;
    uint32_t found = 0;
    AcquireSRWLockShared(&g_mvLock);
    const MvUnit* m = MvUnitOf(entity);
    if (m != NULL)
        for (LONG i = 0; i < g_kitZoneCount; i++)
            if (g_kitZones[i].def.entity == g_mvDefs[m->def - 1].def.entity && g_kitZones[i].slot == zone - 1)
            {
                const int32_t hp = m->kitInit ? m->znHealth[zone - 1] : g_kitZones[i].def.health;
                *out = (HfZoneState){sizeof *out, hp, g_kitZones[i].def.health, hp == 0};
                found = 1;
            }
    ReleaseSRWLockShared(&g_mvLock);
    return found;
}

/* Makes the unit's zone (0: all) whole; the effects of zones still broken stay. 1 when it had one. */
static int ZnRepair(MvUnit* m, uint32_t zone)
{
    int found = 0;
    const uint64_t resource = g_mvDefs[m->def - 1].def.entity;
    KitInit(m);
    for (LONG i = 0; i < g_kitZoneCount; i++)
        if (g_kitZones[i].def.entity == resource && (!zone || g_kitZones[i].slot == zone - 1)) { m->znHealth[g_kitZones[i].slot] = g_kitZones[i].def.health; found = 1; }
    m->znSink = m->znSpin = 0;
    for (LONG i = 0; i < g_kitZoneCount; i++)
        if (g_kitZones[i].def.entity == resource && m->znHealth[g_kitZones[i].slot] == 0) ZnBreak(m, &g_kitZones[i]);
    return found;
}

static int ApiRepairZones(HfMod* mod, uint32_t entity, uint32_t zone)
{
    AcquireSRWLockExclusive(&g_mvLock);
    MvUnit* m = MvUnitOf(entity);
    const int found = m != NULL && ZnRepair(m, zone);
    ReleaseSRWLockExclusive(&g_mvLock);
    if (!found) ApiLog(mod, "repair_zones: unit %u has no such zone", entity);
    return !found;
}

#define MV_KIT_API ApiAddFixedGun, ApiFixedGunState, ApiFixedGunRefill, ApiAddDamageZone, ApiZoneState, ApiRepairZones
