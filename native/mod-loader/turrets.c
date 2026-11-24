/* SDK 37, 39 and 41: turrets (docs/modding/turrets.md, hellforge_turret.h). add_turret is a set of deferred field edits on
 * the game's own records, so the game's turret AI keeps doing the aiming and firing: HellForge never aims a turret
 * itself. The edits go through set_field and mount, so a vanilla entity's shared record is refused there, as for any
 * mod. Included by world_calls.c after movers.c (AiIn). */

#define TR_DEFS 64

typedef struct TrDef { HfMod* mod; HfTurretDef def; int gunDone; } TrDef;
static TrDef g_trDefs[TR_DEFS];
static volatile LONG g_trDefCount;
static void TrLiveStart(void); /* turrets_live.c */

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

/* A limit pair: both 0 keep the game's; else within -bound .. bound, min below max. */
static int TrLimit(float lo, float hi, float bound)
{
    return (lo == 0 && hi == 0) || (AiIn(lo, -bound, bound) && AiIn(hi, -bound, bound) && lo < hi);
}

static int TrSpans(const HfTurretDef* d)
{
    const struct { float v, lo, hi; } spans[] = {
        {d->yaw_speed, 0, 1440}, {d->pitch_speed, 0, 1440}, {d->body_turn_speed, 0, 1440}, {d->pitch_coupling, 0, 10},
        {d->body_turn_accel, 0, 10000}, {d->body_turn_decel, 0, 10000},
        {d->range, 0, 1000}, {d->side_range, 0, 1000}, {d->rear_range, 0, 1000}, {d->aim_spring, 0, 1000},
        {d->aim_damping, 0, 1000}, {d->fire_rate, 0, 6000}};
    for (size_t i = 0; i < sizeof spans / sizeof spans[0]; i++) if (!AiIn(spans[i].v, spans[i].lo, spans[i].hi)) return 0;
    return 1;
}

static int TrHostFits(const HfTurretDef* d)
{
    if (d->face > HF_FACE_AWAY) return 0;
    if (d->host == 0) return !d->mount_slot && !d->attach_node && !d->seat;
    return d->mount_slot <= 4 && d->host != d->entity && d->face == HF_FACE_GAME;
}

/* A seat for PLAYER and BOTH, none for AI and CUSTOM; side and rear sight only shorter than range. */
static int TrAimerFits(const HfTurretDef* d)
{
    const int seated = d->aimer == HF_AIM_PLAYER || d->aimer == HF_AIM_BOTH;
    if (d->aimer > HF_AIM_CUSTOM || (seated != (d->seat != 0))) return 0;
    if (!seated && d->aimer != HF_AIM_AI) return !d->range && !d->side_range && !d->rear_range;
    return d->range ? d->side_range <= d->range && d->rear_range <= d->range : !d->side_range && !d->rear_range;
}

static const char* TrRefusal(const HfTurretDef* in, HfTurretDef* d)
{
    if (in == NULL || in->size < offsetof(HfTurretDef, face) || in->entity == 0)
        return "no HfTurretDef, its entity, or an SDK 37 one (rebuild the mod with SDK 39 or later)";
    memset(d, 0, sizeof *d);
    memcpy(d, in, in->size < sizeof *d ? in->size : sizeof *d);
    if (!TrHostFits(d)) return "mount_slot 0 .. 4 on a host that is not the turret; mount_slot, attach_node and seat need a host; face GAME or AWAY, AWAY without a host";
    if (!TrAimerFits(d))
        return "aimer AI, PLAYER, BOTH or CUSTOM; a seat with PLAYER and BOTH only; ranges only for AI and BOTH, side and rear no longer than range";
    if (!TrSpans(d) || d->rounds > 2047)
        return "turn speeds 0 .. 1440 deg/s, pitch_coupling 0 .. 10, ranges 0 .. 1000 m, aim 0 .. 1000, fire_rate 0 .. 6000, rounds 0 .. 2047";
    if (!TrLimit(d->yaw_min, d->yaw_max, 180) || !TrLimit(d->pitch_min, d->pitch_max, 90))
        return "yaw limits -180 .. 180, pitch limits -90 .. 90, each min below its max (both 0 keep the game's)";
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
        TrF(mod, e, "Rotation", "rotation_acceleration", d->body_turn_accel) | TrF(mod, e, "Rotation", "rotation_deceleration", d->body_turn_decel) |
        TrF(mod, e, "Targeting", "aim_spring", d->aim_spring) | TrF(mod, e, "Targeting", "aim_damping", d->aim_damping);
    if (d->yaw_min != 0 || d->yaw_max != 0)
        failed |= TrSet(mod, e, "Turret", "yaw_min", 0, HF_TYPE_F32, d->yaw_min) | TrSet(mod, e, "Turret", "yaw_max", 0, HF_TYPE_F32, d->yaw_max);
    if (d->pitch_min != 0 || d->pitch_max != 0)
        failed |= TrSet(mod, e, "Turret", "pitch_min", 0, HF_TYPE_F32, d->pitch_min) | TrSet(mod, e, "Turret", "pitch_max", 0, HF_TYPE_F32, d->pitch_max);
    return failed;
}

/* Its sight: one range all round unless side or rear is shorter. PLAYER and CUSTOM shut the sight rather than the
 * behaviour: the behaviour also deploys, animates and feeds the gun, which a seat or a mod still needs. */
static int TrSight(HfMod* mod, const HfTurretDef* d)
{
    const uint64_t e = d->entity;
    const int blind = d->aimer == HF_AIM_PLAYER || d->aimer == HF_AIM_CUSTOM;
    if (blind)
        return TrSet(mod, e, "SensorEye", "range", 0, HF_TYPE_F32, 0) | TrSet(mod, e, "SensorEye", "side_range", 0, HF_TYPE_F32, 0) |
            TrSet(mod, e, "SensorEye", "rear_range", 0, HF_TYPE_F32, 0);
    return TrF(mod, e, "SensorEye", "range", d->range) | TrF(mod, e, "SensorEye", "side_range", d->side_range ? d->side_range : d->range) |
        TrF(mod, e, "SensorEye", "rear_range", d->rear_range ? d->rear_range : d->range);
}

/* The magazine, from add_turret. Fire rate and projectile wait for the data (TrGunReady): which weapon record the gun
 * has, a beam's or a projectile's, says where they go. */
static int TrGun(HfMod* mod, const HfTurretDef* d)
{
    const uint64_t gun = d->gun ? d->gun : d->entity;
    return d->rounds ? TrSet(mod, gun, "WeaponMagazine", "capacity", 0, HF_TYPE_U32, d->rounds) : 0;
}

/* What the gun lacks for its settings, or NULL. */
static const char* TrGunLacks(const HfTurretDef* d, int beam, int shots)
{
    if (d->projectile && !shots) return "projectile weapon";
    return d->fire_rate && !beam && !shots ? "weapon" : NULL;
}

static void TrGunEdits(TrDef* t)
{
    const HfTurretDef* d = &t->def;
    const uint64_t gun = d->gun ? d->gun : d->entity;
    uint32_t stride = 0;
    const int beam = ComponentRecordOf("BeamWeaponComponentData", gun, &stride, 1) != 0;
    const int shots = ComponentRecordOf("ProjectileWeaponComponentData", gun, &stride, 1) != 0;
    const char* lacks = TrGunLacks(d, beam, shots);
    t->gunDone = 1;
    if (d->fire_rate && beam) TrSet(t->mod, gun, "BeamWeapon", "fire_rate", 0, HF_TYPE_I32, (double)(int32_t)(d->fire_rate + 0.5f));
    else if (d->fire_rate && shots) TrSet(t->mod, gun, "ProjectileWeapon", "fire_rate", 1, HF_TYPE_F32, d->fire_rate);
    if (d->projectile && shots) TrSet(t->mod, gun, "ProjectileWeapon", "projectile", 0, HF_TYPE_U32, d->projectile);
    if (lacks) ApiLog(t->mod, "add_turret(%016llx): its gun %016llx has no %s record, so some settings were not set", (unsigned long long)d->entity, (unsigned long long)gun, lacks);
}

/* hellforge_modloader.c's Fire, when the entity data is in: each turret's gun edits, once. */
static void TrGunReady(void)
{
    for (LONG i = 0; i < g_trDefCount && i < TR_DEFS; i++)
        if (g_trDefs[i].def.entity && !g_trDefs[i].gunDone && !ModGone(g_trDefs[i].mod)) TrGunEdits(&g_trDefs[i]);
}

/* SDK 37 and 39, add_turret: the turret's settings, as deferred edits of its records (hellforge_turret.h). From
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
    g_trDefs[at] = (TrDef){mod, d, 0};
    TrLiveStart();
    if (g_fired[HF_EVENT_ENTITIES_READY]) TrGunEdits(&g_trDefs[at]);
    const int failed = TrMount(mod, &d) | TrAim(mod, &d) | TrSight(mod, &d) | TrGun(mod, &d);
    static const char* aimers[] = {"the game's AI", "the seated player", "the seated player, else the AI", "the mod's code"};
    ApiLog(mod, "add_turret(%016llx): aimed by %s, turns %.0f / %.0f deg/s, sees %.0f m (0 = the game's), %.0f a minute%s%s",
        (unsigned long long)d.entity, aimers[d.aimer], (double)d.yaw_speed, (double)d.pitch_speed, (double)d.range, (double)d.fire_rate,
        d.host ? ", on a host's mount slot" : d.face == HF_FACE_AWAY ? ", facing away from its caller" : "", failed ? "; some settings were refused (lines above)" : "");
    if (d.seat) ApiLog(mod, "add_turret(%016llx): seat %u kept; seats aim turrets from a later SDK", (unsigned long long)d.entity, d.seat);
    return 0;
}

/* The table's entries from SDK 37 on (ApiInit). */
#define TR_API ApiAddTurret
