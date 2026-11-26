/* The class line of an Armory item's CUSTOMIZE screen (game.dll 0x18DC920: (u32* text id, loadout id)). The game finds
 * the item in the stratagems, then the armory object's types 1..6, then a list of its own, and answers 0 (UNKNOWN) for
 * the rest. A mod's item that still reads 0 is looked up in HellForge's armory object, every type, and gets the text
 * of its kind, as the game gives a vanilla row's. Included by list_filter.c (LF_KIND, LF_KIND_TABLE, g_lfKinds). */

#define CC_CLASS 0x18DC920u
#define CC_LOGGED 16u

typedef uint32_t* (*CcClassFn)(uint32_t* out, uint32_t item);
static CcClassFn g_ccClass;
static const uint8_t kCcClassPin[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x41, 0xBA, 0x01, 0x00, 0x00, 0x00, 0x4C, 0x8D, 0x05};
static uint32_t g_ccLogged[CC_LOGGED];
static volatile LONG g_ccLogCount;

/* Logged once per item, at most CC_LOGGED items: the in-game check of which items the game could not name. */
static void CcNote(uint32_t item, int type, uint32_t id)
{
    for (LONG i = 0; i < g_ccLogCount && i < (LONG)CC_LOGGED; i++)
        if (g_ccLogged[i] == item) return;
    const LONG at = InterlockedIncrement(&g_ccLogCount) - 1;
    if (at >= (LONG)CC_LOGGED) return;
    g_ccLogged[at] = item;
    if (type < 0) Note("customize: item %08x has no class: not in HellForge's armory rows", item);
    else Note("customize: item %08x had no class from the game; HellForge's armory row (type %d) gives text %08x", item, type, id);
}

/* The text id of a row's kind, as the game reads it for a vanilla row. */
static uint32_t CcKindText(const uint8_t* row)
{
    uint64_t entity;
    uint32_t kind = 0x20, text = 0;
    memcpy(&entity, row + 8, 8);
    const uint8_t* record = entity ? ((LfRecordFn)(g_lfGame + LF_KIND))(entity) : NULL;
    if (record == NULL || SafeRead((uintptr_t)record + 0x80, &kind, 4) || kind > 0x20) kind = 0x20;
    return SafeRead(g_lfGame + LF_KIND_TABLE + kind * 4u, &text, 4) ? 0 : text;
}

/* The text id of an item's kind, from HellForge's armory object; 0 when it has no row there. type gets the row's type. */
static uint32_t CcRowClass(uint32_t item, int* type)
{
    *type = -1;
    if (g_armory == NULL || !g_lfKinds) return 0;
    uint32_t starts[REG_ARMORY_TYPES], counts[REG_ARMORY_TYPES];
    memcpy(starts, g_armory + 0x1B24, sizeof starts);
    memcpy(counts, g_armory + 0x1B40, sizeof counts);
    for (int t = 0; t < REG_ARMORY_TYPES; t++)
        for (uint32_t k = 0; k < counts[t]; k++)
        {
            const uint8_t* row = g_armory + (size_t)(starts[t] + k) * 32;
            if (memcmp(row, &item, 4) != 0) continue;
            *type = t;
            return CcKindText(row);
        }
    return 0;
}

static uint32_t* CcClass(uint32_t* out, uint32_t item)
{
    uint32_t* r = g_ccClass(out, item);
    if (r == NULL || *r != 0 || item == 0) return r;
    int type;
    const uint32_t text = CcRowClass(item, &type);
    CcNote(item, type, text);
    if (text) *r = text;
    return r;
}

static void CcInstall(uintptr_t game)
{
    const char* why = !g_lfKinds ? "the kind reads are not the reviewed ones" :
                      !PinMatches(game + CC_CLASS, kCcClassPin, sizeof kCcClassPin) ? "its code is not the reviewed one" :
                      LoaderHook("lists: the CUSTOMIZE class line", game + CC_CLASS, (void*)CcClass, (void**)&g_ccClass) ? "it could not be hooked" : NULL;
    if (why) Note("customize: mods' items keep UNKNOWN as their CUSTOMIZE class (%s)", why);
}
