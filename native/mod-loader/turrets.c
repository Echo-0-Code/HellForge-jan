/* SDK 37: turrets (docs/modding/turrets.md, hellforge_turret.h). add_turret is a set of deferred field edits on the
 * game's own records, so the game's turret AI keeps doing the aiming and firing: HellForge never aims a turret itself.
 * The edits go through set_field and mount, so a vanilla entity's shared record is refused there, as for any mod.
 * Included by world_calls.c after movers.c (AiIn). */

#define TR_DEFS 64

typedef struct TrDef { HfMod* mod; HfTurretDef def; } TrDef;
static TrDef g_trDefs[TR_DEFS];
static volatile LONG g_trDefCount;

/* One field edit by the mappings' names: 0 queued, 1 refused (set_field logs why). */
static int TrSet(HfMod* mod, uint64_t entity, const char* component, const char* name, uint16_t element, uint32_t type, double value)
{
    const SdkComponent* c = SdkComponentNamed(component);
    const SdkField* f = SdkFieldNamed(c, name);
    if (f == NULL) { ApiLog(mod, "add_turret: HellForge has no mappings for %s.%s", component, name); return 1; }
    const uint16_t size = (uint16_t)TypeSize(type);
    const HfField field = {c->component, c->layout, (uint16_t)(f->offset + element * size), size, type, 0};
    const HfValue v = {type, value, 0};
    return ApiSetField(mod, entity, &field, &v);
}

static int TrF(HfMod* mod, uint64_t entity, const char* component, const char* name, float value)
{
    return value != 0 ? TrSet(mod, entity, component, name, 0, HF_TYPE_F32, value) : 0;
}

static int TrSpans(const HfTurretDef* d)
{
    const struct { float v, lo, hi; } spans[] = {
        {d->yaw_speed, 0, 1440}, {d->pitch_speed, 0, 1440}, {d->body_turn_speed, 0, 1440}, {d->pitch_coupling, 0, 10},
        {d->range, 0, 1000}, {d->side_range, 0, 1000}, {d->rear_range, 0, 1000}, {d->aim_spring, 0, 1000},
        {d->aim_damping, 0, 1000}, {d->fire_rate, 0, 6000}};
    for (size_t i = 0; i < sizeof spans / sizeof spans[0]; i++) if (!AiIn(spans[i].v, spans[i].lo, spans[i].hi)) return 0;
    return 1;
}

static int TrLimit(uint32_t limits, uint32_t flag, float lo, float hi, float bound)
{
    return !(limits & flag) || (AiIn(lo, -bound, bound) && AiIn(hi, -bound, bound) && lo < hi);
}

static int TrHostFits(const HfTurretDef* d)
{
    if (d->host == 0) return !d->mount_slot && !d->attach_node && !d->seat;
    return d->mount_slot <= 4 && d->host != d->entity;
}

static const char* TrRefusal(const HfTurretDef* in, HfTurretDef* d)
{
    if (in == NULL || in->size < sizeof *in || in->entity == 0) return "no HfTurretDef, its size or its entity";
    *d = *in;
    if (d->ai > HF_TURRET_AI_OFF || d->weapon > HF_TURRET_WEAPON_BEAM || d->limits > 3) return "ai NATIVE or OFF, weapon PROJECTILE or BEAM, limits YAW | PITCH";
    if (!TrHostFits(d)) return "mount_slot 0 .. 4 on a host that is not the turret; mount_slot, attach_node and seat need a host";
    if (!TrSpans(d) || d->rounds > 2047)
        return "turn speeds 0 .. 1440 deg/s, pitch_coupling 0 .. 10, ranges 0 .. 1000 m, aim 0 .. 1000, fire_rate 0 .. 6000, rounds 0 .. 2047";
    if (!TrLimit(d->limits, HF_TURRET_LIMIT_YAW, d->yaw_min, d->yaw_max, 180) || !TrLimit(d->limits, HF_TURRET_LIMIT_PITCH, d->pitch_min, d->pitch_max, 90))
        return "yaw limits -180 .. 180, pitch limits -90 .. 90, each min below its max";
    if (d->ai == HF_TURRET_AI_OFF && (d->range || d->side_range || d->rear_range)) return "ranges are for the AI: leave them 0 with ai OFF";
    return NULL;
}

/* The host's mount slot and node. */
static int TrMount(HfMod* mod, const HfTurretDef* d)
{
    char node[32];
    if (!d->host) return 0;
    snprintf(node, sizeof node, "slot%u_attach_node", d->mount_slot);
    return ApiMount(mod, d->host, d->mount_slot, d->entity) | (d->attach_node ? TrSet(mod, d->host, "Mount", node, 0, HF_TYPE_U32, d->attach_node) : 0);
}

/* Turning, limits and aim, on the entity that aims. */
static int TrAim(HfMod* mod, const HfTurretDef* d)
{
    const uint64_t e = d->entity;
    int failed = TrF(mod, e, "Turret", "yaw_speed", d->yaw_speed) | TrF(mod, e, "Turret", "pitch_speed", d->pitch_speed) |
        TrF(mod, e, "Turret", "pitch_yaw_coupling", d->pitch_coupling) | TrF(mod, e, "Rotation", "rotation_turn_speed", d->body_turn_speed) |
        TrF(mod, e, "Targeting", "aim_spring", d->aim_spring) | TrF(mod, e, "Targeting", "aim_damping", d->aim_damping);
    if (d->limits & HF_TURRET_LIMIT_YAW)
        failed |= TrSet(mod, e, "Turret", "yaw_min", 0, HF_TYPE_F32, d->yaw_min) | TrSet(mod, e, "Turret", "yaw_max", 0, HF_TYPE_F32, d->yaw_max);
    if (d->limits & HF_TURRET_LIMIT_PITCH)
        failed |= TrSet(mod, e, "Turret", "pitch_min", 0, HF_TYPE_F32, d->pitch_min) | TrSet(mod, e, "Turret", "pitch_max", 0, HF_TYPE_F32, d->pitch_max);
    return failed;
}

/* Its sight. AI off shuts the sight rather than the behaviour: the behaviour also deploys, animates and feeds the
 * gun, which a seat or a mod still needs. */
static int TrSight(HfMod* mod, const HfTurretDef* d)
{
    const uint64_t e = d->entity;
    if (d->ai == HF_TURRET_AI_OFF)
        return TrSet(mod, e, "SensorEye", "range", 0, HF_TYPE_F32, 0) | TrSet(mod, e, "SensorEye", "side_range", 0, HF_TYPE_F32, 0) |
            TrSet(mod, e, "SensorEye", "rear_range", 0, HF_TYPE_F32, 0);
    return TrF(mod, e, "SensorEye", "range", d->range) | TrF(mod, e, "SensorEye", "side_range", d->side_range) |
        TrF(mod, e, "SensorEye", "rear_range", d->rear_range);
}

/* Fire rate and magazine, on the entity whose weapon fires. */
static int TrGun(HfMod* mod, const HfTurretDef* d)
{
    const uint64_t gun = d->gun ? d->gun : d->entity;
    int failed = d->rounds ? TrSet(mod, gun, "WeaponMagazine", "capacity", 0, HF_TYPE_U32, d->rounds) : 0;
    if (d->fire_rate && d->weapon == HF_TURRET_WEAPON_BEAM)
        failed |= TrSet(mod, gun, "BeamWeapon", "fire_rate", 0, HF_TYPE_I32, (double)(int32_t)(d->fire_rate + 0.5f));
    else if (d->fire_rate)
        failed |= TrSet(mod, gun, "ProjectileWeapon", "fire_rate", 1, HF_TYPE_F32, d->fire_rate);
    return failed;
}

/* SDK 37, add_turret: the turret's settings, as deferred edits of its records (hellforge_turret.h). From
 * hellforge_mod_init, so a mod's own entity gets records of its own to edit. 0 done, 1 refused (the log says why). */
static int ApiAddTurret(HfMod* mod, const HfTurretDef* in)
{
    HfTurretDef d;
    const char* why = TrRefusal(in, &d);
    if (why) { ApiLog(mod, "add_turret: refused: %s", why); return 1; }
    for (LONG i = 0; i < g_trDefCount; i++)
        if (g_trDefs[i].def.entity == d.entity) { ApiLog(mod, "add_turret(%016llx): refused: it is a turret already", (unsigned long long)d.entity); return 1; }
    LONG at = InterlockedIncrement(&g_trDefCount) - 1;
    if (at >= TR_DEFS) { InterlockedDecrement(&g_trDefCount); ApiLog(mod, "add_turret: refused: %d turrets already", TR_DEFS); return 1; }
    g_trDefs[at] = (TrDef){mod, d};
    const int failed = TrMount(mod, &d) | TrAim(mod, &d) | TrSight(mod, &d) | TrGun(mod, &d);
    ApiLog(mod, "add_turret(%016llx): %s AI, turns %.0f / %.0f deg/s, sees %.0f m (0 = the game's), %.0f a minute%s%s",
        (unsigned long long)d.entity, d.ai == HF_TURRET_AI_OFF ? "no" : "the game's", (double)d.yaw_speed, (double)d.pitch_speed,
        (double)d.range, (double)d.fire_rate, d.host ? ", on a host's mount slot" : "", failed ? "; some settings were refused (lines above)" : "");
    if (d.seat) ApiLog(mod, "add_turret(%016llx): seat %u kept; seats aim turrets from a later SDK", (unsigned long long)d.entity, d.seat);
    else if (d.ai == HF_TURRET_AI_OFF) ApiLog(mod, "add_turret(%016llx): no AI and no seat: only the mod's code aims it", (unsigned long long)d.entity);
    return 0;
}

/* The table's entries from SDK 37 on (ApiInit). */
#define TR_API ApiAddTurret
