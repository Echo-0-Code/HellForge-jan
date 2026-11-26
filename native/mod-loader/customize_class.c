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
static void CcNote(uint32_t item, int type, uint32_t id, uintptr_t caller)
{
    for (LONG i = 0; i < g_ccLogCount && i < (LONG)CC_LOGGED; i++)
        if (g_ccLogged[i] == item) return;
    const LONG at = InterlockedIncrement(&g_ccLogCount) - 1;
    if (at >= (LONG)CC_LOGGED) return;
    g_ccLogged[at] = item;
    if (type < 0) Note("customize: item %08x has no class: not in HellForge's armory rows (asked from game.dll+%llx)", item, (unsigned long long)(caller - g_lfGame));
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

static int CcFirst(int list, uint32_t id); /* the probe's, below */
static uint32_t* CcClass(uint32_t* out, uint32_t item)
{
    uint32_t* r = g_ccClass(out, item);
    if (r != NULL && *r != 0 && CcFirst(2, item))
        Note("customize probe: class of %08x from game.dll+%llx: text %08x", item,
             (unsigned long long)((uintptr_t)__builtin_return_address(0) - g_lfGame), *r);
    if (r == NULL || *r != 0 || item == 0) return r;
    int type;
    const uint32_t text = CcRowClass(item, &type);
    CcNote(item, type, text, (uintptr_t)__builtin_return_address(0));
    if (text) *r = text;
    return r;
}

/* Diagnostics for the CUSTOMIZE header (Jan, 26 Nov: probe before a fix). The item tile builder 0x14EB900 finds its
 * item record the way it does at 0x14EB94D (key [tile+0x388] in the 0x18-byte rows at [[0x347CEF8]+0x54D2DF4], record
 * index at row +0, 0xB8-byte records at +0x4DA2DF4); its class branch at 0x14EC0AD reads the armory row (0x11E7C60, called
 * here, not hooked) +0x10 type. 0x18DDAD0's shown items and their answers are logged too. Once per id, log only. */
#define CC_ITEM_CLASS 0x18DDAD0u
#define CC_TILE 0x14EB900u
#define CC_ROW_OF 0x11E7C60u
#define CC_MENU 0x347CEF8u
#define CC_SEEN 32u
typedef uint32_t* (*CcItemClassFn)(uint32_t* out, const uint8_t* item);
typedef void* (*CcTileFn)(uint8_t* tile, void* a, void* b, void* c);
typedef const uint8_t* (*CcRowOfFn)(uint32_t id);
static CcItemClassFn g_ccItemClass;
static CcTileFn g_ccTile;
static const uint8_t kCcItemClassPin[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x20};
static const uint8_t kCcTilePin[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48, 0x89, 0x7C, 0x24, 0x20, 0x55};
static const uint8_t kCcRowOfPin[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x8B, 0x1D};
static uint32_t g_ccSeen[3][CC_SEEN];
static volatile LONG g_ccSeenCount[3];

/* 1 the first time list (0 shown items, 1 tiles, 2 class answers) meets id, while there is room. */
static int CcFirst(int list, uint32_t id)
{
    for (LONG i = 0; i < g_ccSeenCount[list] && i < (LONG)CC_SEEN; i++)
        if (g_ccSeen[list][i] == id) return 0;
    const LONG at = InterlockedIncrement(&g_ccSeenCount[list]) - 1;
    if (at >= (LONG)CC_SEEN) return 0;
    g_ccSeen[list][at] = id;
    return 1;
}

static void CcHex(const uint8_t* bytes, size_t n, char* out)
{
    for (size_t k = 0; k < n; k++) snprintf(out + 2 * k, 3, "%02x", bytes[k]);
}

static uint32_t* CcItemClass(uint32_t* out, const uint8_t* item)
{
    uint32_t* r = g_ccItemClass(out, item);
    uint8_t head[0x20];
    uint32_t id;
    if (item == NULL || SafeRead((uintptr_t)item, head, sizeof head)) return r;
    memcpy(&id, head + 8, 4);
    if (!CcFirst(0, id)) return r;
    char hex[sizeof head * 2 + 1];
    CcHex(head, sizeof head, hex);
    Note("customize probe: shown item %08x from game.dll+%llx: text %08x; item +0..0x20 %s", id,
         (unsigned long long)((uintptr_t)__builtin_return_address(0) - g_lfGame), r ? *r : 0, hex);
    return r;
}

/* The tile's item record, found as 0x14EB900 finds it; 0 when it has none. */
static uintptr_t CcTileRecord(const uint8_t* tile, uint32_t* key)
{
    uintptr_t menu = 0, rows;
    uint32_t count = 0;
    if (SafeRead((uintptr_t)tile + 0x388, key, 4) || !*key || SafeRead(g_lfGame + CC_MENU, &menu, 8) || !menu ||
        SafeRead(menu + 0x4DA2DF0, &count, 4))
        return 0;
    rows = menu + 0x54D2DF4;
    for (uint32_t i = 0; i < count && i < 65536; i++)
    {
        uint32_t row[2];
        if (SafeRead(rows + (uintptr_t)i * 0x18, row, 8)) return 0;
        if (row[1] == *key) return menu + 0x4DA2DF4 + (uintptr_t)row[0] * 0xB8;
    }
    return 0;
}

static void CcTileNote(const uint8_t* tile)
{
    uint32_t key = 0, slot = 0, id = 0;
    const uintptr_t record = CcTileRecord(tile, &key);
    if (record == 0 || SafeRead(record + 8, &id, 4) || !CcFirst(1, id)) return;
    SafeRead((uintptr_t)tile + 0x390, &slot, 4);
    uint8_t head[0x20], rowBytes[0x18] = {0};
    char hex[sizeof head * 2 + 1], rowHex[sizeof rowBytes * 2 + 1];
    if (SafeRead(record, head, sizeof head)) memset(head, 0, sizeof head);
    const uint8_t* row = ((CcRowOfFn)(g_lfGame + CC_ROW_OF))(id);
    if (row != NULL) memcpy(rowBytes, row, sizeof rowBytes);
    uint32_t type = 0;
    memcpy(&type, rowBytes + 0x10, 4);
    CcHex(head, sizeof head, hex);
    CcHex(rowBytes, sizeof rowBytes, rowHex);
    Note("customize probe: tile key %08x (+0x390 %u): item %08x, record +0..0x20 %s; armory row %s %s, type %u (1 and 3 ask 0x18DDAD0, 4 reads 4df8fdcf, others none)",
         key, slot, id, hex, row ? "found" : "none", rowHex, type);
}

static void* CcTile(uint8_t* tile, void* a, void* b, void* c)
{
    if (tile != NULL && g_ccSeenCount[1] < (LONG)CC_SEEN) CcTileNote(tile);
    return g_ccTile(tile, a, b, c);
}

static void CcProbeInstall(uintptr_t game)
{
    const int a = PinMatches(game + CC_ITEM_CLASS, kCcItemClassPin, sizeof kCcItemClassPin) &&
                  !LoaderHook("lists: CUSTOMIZE probe (shown item)", game + CC_ITEM_CLASS, (void*)CcItemClass, (void**)&g_ccItemClass);
    const int b = PinMatches(game + CC_TILE, kCcTilePin, sizeof kCcTilePin) && PinMatches(game + CC_ROW_OF, kCcRowOfPin, sizeof kCcRowOfPin) &&
                  !LoaderHook("lists: CUSTOMIZE probe (item tile)", game + CC_TILE, (void*)CcTile, (void**)&g_ccTile);
    Note("customize probe: shown items %s, item tiles %s", a ? "on" : "off", b ? "on" : "off");
}

static void CcInstall(uintptr_t game)
{
    CcProbeInstall(game);
    const char* why = !g_lfKinds ? "the kind reads are not the reviewed ones" :
                      !PinMatches(game + CC_CLASS, kCcClassPin, sizeof kCcClassPin) ? "its code is not the reviewed one" :
                      LoaderHook("lists: the CUSTOMIZE class line", game + CC_CLASS, (void*)CcClass, (void**)&g_ccClass) ? "it could not be hooked" : NULL;
    if (why) Note("customize: mods' items keep UNKNOWN as their CUSTOMIZE class (%s)", why);
}
