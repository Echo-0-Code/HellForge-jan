/* Some game systems find a resource by an entity's id: a vanilla id is its path's hash, which also names its unit and
 * the unit's companions (physics, state machine, bones, effects). The minefield's thrower (game.dll 0x6CB140) spawns
 * mines with spawn_unit named by ThrowerComponentData +0x00. A copy's id names nothing, so each id is overridden to its
 * unit record's unit, for "unit" and every other type that unit's name has (kCopyNameTypes). The engine follows
 * override chains (exe 0x5F2E80). A type the id already names is kept; weapons need none (their id is their unit's).
 * On the game thread (packages.c TickDetour): elsewhere set_resource_override faults (exe +0x669DEB). */

#define CU_CHECK_MS 1000u

/* The resource types a copy's id is pointed at its unit's for, "unit" first. Left out: "entity" (the copies' own
 * tables), and code and set-up types no entity is named by (package, lua, level, font, strings). */
static const char* const kCopyNameTypes[] = {"unit", "physics", "state_machine", "bones", "animation", "ragdoll_profile",
    "geometry_group", "material", "texture", "particles", "prefab", "wwise_bank", "wwise_dep", "wwise_stream"};
#define CU_TYPES ((int)(sizeof kCopyNameTypes / sizeof kCopyNameTypes[0]))

typedef struct CopyNames
{
    uint64_t unit;    /* the unit entity's id was pointed at (0: none yet; the id itself: named after its own unit) */
    uint16_t set;     /* the types (bits of kCopyNameTypes) the id is pointed at that unit's for */
    uint16_t checked; /* 1 once the unit was loaded and its other types looked at */
} CopyNames;

static CopyNames* g_copyNames; /* one per g_weapons entry */
static int g_copyNamesCap;
static int g_copyUnitRefused;  /* the engine's override functions are not the reviewed ones */
static uint64_t g_copyTypes[CU_TYPES];
/* The unit record lookup (the native fixture replaces it). */
static uintptr_t (*g_copyUnitRecord)(const char* name, uint64_t resource, uint32_t* stride, int shared) = ComponentRecordOf;

/* The unit entity i draws, from its UnitComponentData record (its own or the one it shares), or 0. */
static uint64_t CopyUnitOf(int i)
{
    uint32_t stride = 0;
    uint64_t unit = 0;
    uintptr_t record = g_copyUnitRecord("UnitComponentData", g_weapons[i].entity, &stride, 1);
    if (record == 0 || SafeRead(record, &unit, 8)) return 0;
    return unit;
}

/* The type names of a bit set, comma-separated. */
static void CopyTypeNames(uint32_t bits, char* out, size_t size)
{
    size_t used = 0;
    out[0] = 0;
    for (int t = 0; t < CU_TYPES && used + 1 < size; t++)
        if (bits & (1u << t)) used += (size_t)snprintf(out + used, size - used, "%s%s", used ? ", " : "", kCopyNameTypes[t]);
}

/* The unit's other types the id is pointed at, once that unit is loaded (keep: those set for it already). */
static uint32_t CopyCompanions(uint64_t entity, uint64_t unit, uint32_t keep)
{
    uint32_t bits = 0;
    for (int t = 1; t < CU_TYPES; t++)
    {
        uint32_t bit = 1u << t;
        if (keep & bit) { bits |= bit; continue; }
        if (!g_hotCanGet(g_copyTypes[t], unit) || g_hotCanGet(g_copyTypes[t], entity)) continue;
        g_hotOverride(g_copyTypes[t], entity, unit);
        bits |= bit;
    }
    return bits;
}

/* Entity i's id needs nothing of its own: named after its unit, or a unit of that name is loaded already. 1 then. */
static int CopyNamedAlready(int i, uint64_t unit)
{
    const Weapon* w = &g_weapons[i];
    CopyNames* c = &g_copyNames[i];
    if (unit != w->entity && (c->unit != 0 || !g_hotCanGet(g_copyTypes[0], w->entity))) return 0;
    if (unit != w->entity)
        ApiLog(w->mod, "%s: a unit named %016llx is loaded already; the game draws that one where it spawns this entity by its id",
            w->name, (unsigned long long)w->entity);
    c->unit = w->entity;
    c->checked = 1;
    return 1;
}

/* The types newly pointed at the unit, in the mod's log. */
static void CopyNamesLog(const Weapon* w, uint64_t unit, uint32_t added, int changed)
{
    if (added == 0) return;
    char types[256];
    CopyTypeNames(added, types, sizeof types);
    ApiLog(w->mod, "%s: where the game finds a resource by this entity's id (%016llx), it finds unit %016llx's (%s)%s", w->name,
        (unsigned long long)w->entity, (unsigned long long)unit, types, changed ? " (its unit record changed)" : "");
}

/* Entity i's id pointed at its unit's resources, when it needs it and that changed. A type the old unit had and the
 * new one lacks is cleared, so a lookup never finds the old model's companion. */
static void CopyUnitName(int i)
{
    const Weapon* w = &g_weapons[i];
    CopyNames* c = &g_copyNames[i];
    if (!w->plain || w->state != 1 || LiveGone(LC_WEAPON, (uint32_t)i)) return;
    uint64_t unit = CopyUnitOf(i);
    if (unit == 0 || (unit == c->unit && c->checked) || CopyNamedAlready(i, unit)) return;
    int changed = c->unit != 0 && unit != c->unit;
    uint32_t keep = changed ? 0u : c->set, want = 1u;
    if (!(keep & 1u)) g_hotOverride(g_copyTypes[0], w->entity, unit);
    c->checked = g_hotCanGet(g_copyTypes[0], unit) != 0;
    want |= c->checked ? CopyCompanions(w->entity, unit, keep) : keep;
    uint32_t gone = changed ? c->set & ~want & ~1u : 0u;
    for (int t = 1; t < CU_TYPES; t++) if (gone & (1u << t)) g_hotClear(g_copyTypes[t], w->entity);
    CopyNamesLog(w, unit, want & ~keep, changed);
    c->unit = unit;
    c->set = (uint16_t)want;
}

/* The engine's override functions proven and the type names hashed, once. 0 when refused. */
static int CopyNamesReady(void)
{
    if (g_copyTypes[0] != 0) return 1;
    char why[192];
    if (!HotProve(why, sizeof why))
    {
        g_copyUnitRefused = 1;
        Note("copy units: %s; entities spawned by their id (minefield mines) draw the missing-unit placeholder", strchr(why, '\t') ? strchr(why, '\t') + 1 : why);
        return 0;
    }
    for (int t = 0; t < CU_TYPES; t++) g_copyTypes[t] = Murmur64A(kCopyNameTypes[t], strlen(kCopyNameTypes[t]));
    return 1;
}

/* On the game thread once the entity data is ready: every registered entity's id names the resources of the unit it
 * draws. */
static void CopyUnitNamesTick(void)
{
    static ULONGLONG last;
    static int lastCount;
    if (!g_fired[HF_EVENT_ENTITIES_READY] || g_copyUnitRefused || g_weaponCount == 0) return;
    ULONGLONG now = GetTickCount64();
    if (lastCount == g_weaponCount && now - last < CU_CHECK_MS) return;
    last = now;
    lastCount = g_weaponCount;
    if (!CopyNamesReady()) return;
    int count = g_weaponCount;
    if (count > g_copyNamesCap)
    {
        CopyNames* grown = (CopyNames*)realloc(g_copyNames, (size_t)count * sizeof *grown);
        if (grown == NULL) return;
        memset(grown + g_copyNamesCap, 0, (size_t)(count - g_copyNamesCap) * sizeof *grown);
        g_copyNames = grown;
        g_copyNamesCap = count;
    }
    for (int i = 0; i < count; i++) CopyUnitName(i);
}
