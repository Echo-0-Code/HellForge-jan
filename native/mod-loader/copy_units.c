/* Some game systems spawn an entity's model by the entity's id, not its UnitComponentData record: a vanilla id is the
 * hash of its unit's name. The minefield's thrower (game.dll 0x6CB140, 0x6CEA10) spawns each mine with spawn_unit
 * ([[game + 0x3326308] + 8] + 8) named by ThrowerComponentData +0x00. An add_entity copy's id names no unit, so the
 * engine drew its missing-unit placeholder. So each copy's id gets a "unit" resource override (live_data.c) to the unit
 * its record names; the engine follows override chains (exe 0x5F2E80). Weapons need none: their id is their unit's
 * name. Rechecked each second for live changes and late unit edits; copies live all session, so nothing is cleared. */

#define CU_CHECK_MS 1000u

static uint64_t* g_copyUnitSet; /* the unit each entity's id was pointed at (0: none yet), one per g_weapons entry */
static int g_copyUnitCap;
static int g_copyUnitRefused;                   /* the engine's override functions are not the reviewed ones */
/* The unit record lookup and the clock (the native fixture replaces them). */
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

/* Entity i's id pointed at its unit, when it needs it and that changed. */
static void CopyUnitName(int i, uint64_t unitType)
{
    const Weapon* w = &g_weapons[i];
    if (!w->plain || w->state != 1 || LiveGone(LC_WEAPON, (uint32_t)i)) return;
    uint64_t unit = CopyUnitOf(i);
    if (unit == 0 || unit == g_copyUnitSet[i]) return;
    if (unit == w->entity) { g_copyUnitSet[i] = unit; return; } /* named after its unit, as a vanilla entity is */
    if (g_copyUnitSet[i] == 0 && g_hotCanGet(unitType, w->entity))
    {   /* a unit of that name is loaded already: it is the entity's, never redirect it */
        g_copyUnitSet[i] = w->entity;
        ApiLog(w->mod, "%s: a unit named %016llx is loaded already; the game draws that one where it spawns this entity by its id",
            w->name, (unsigned long long)w->entity);
        return;
    }
    g_hotOverride(unitType, w->entity, unit);
    ApiLog(w->mod, "%s: where the game spawns this entity's model by its id (%016llx), it draws unit %016llx%s", w->name,
        (unsigned long long)w->entity, (unsigned long long)unit, g_copyUnitSet[i] ? " (its unit record changed)" : "");
    g_copyUnitSet[i] = unit;
}

/* On the watch thread once the entity data is ready: every registered entity's id names the unit it draws. */
static void CopyUnitNamesTick(void)
{
    static ULONGLONG last;
    static int lastCount;
    static uint64_t unitType;
    if (!g_fired[HF_EVENT_ENTITIES_READY] || g_copyUnitRefused || g_weaponCount == 0) return;
    ULONGLONG now = GetTickCount64();
    if (lastCount == g_weaponCount && now - last < CU_CHECK_MS) return;
    last = now;
    lastCount = g_weaponCount;
    if (unitType == 0)
    {
        char why[192];
        if (!HotProve(why, sizeof why))
        {
            g_copyUnitRefused = 1;
            Note("copy units: %s; entities spawned by their id (minefield mines) draw the missing-unit placeholder", strchr(why, '\t') ? strchr(why, '\t') + 1 : why);
            return;
        }
        unitType = Murmur64A("unit", 4);
    }
    int count = g_weaponCount;
    if (count > g_copyUnitCap)
    {
        uint64_t* grown = (uint64_t*)realloc(g_copyUnitSet, (size_t)count * sizeof *grown);
        if (grown == NULL) return;
        memset(grown + g_copyUnitCap, 0, (size_t)(count - g_copyUnitCap) * sizeof *grown);
        g_copyUnitSet = grown;
        g_copyUnitCap = count;
    }
    for (int i = 0; i < count; i++) CopyUnitName(i, unitType);
}
