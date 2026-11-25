/* SDK 38: the game's item lists (hellforge_lists.h, docs/modding/lists.md), for game.dll build 25480438.
 * Armory weapon grids: the builder skips a row whose item is 0, so HellForge zeroes the hidden rows' items while it
 * runs and puts them back after. Stratagem picker: its walk over the types asks "held?" per type at one call site;
 * that call points here and also answers no for a hidden type. Included by world_calls.c after item_traits.c. */
#include "hellforge_lists.h"

#define LF_GRID_BUILD 0x18D7360u  /* (grid, type): the Armory weapon page's grid */
#define LF_ROWS_READ 0x18D7746u   /* the builder's read of the armory object's row count */
#define LF_TAB_SELECT 0x14570B0u  /* (screen, tab index): the weapon page's tab select, which builds the grid again */
#define LF_ARMORY 0x33269E8u      /* the armory object: count at +0x1B40+type*4, first row at +0x1B24+type*4 */
#define LF_MENU 0x347CE38u        /* +0 the menu state (2 the armory), +0x60 the armory screen */
#define LF_ARMORY_GRID 0x7F718u  /* the screen's weapon grid (LF_GRID_BUILD's first argument) */
#define LF_TAB_TYPES 0x479C8u     /* the screen's tab types (u32, 0x3010 apart) */
#define LF_GRID_TYPE 0x1F83ACu    /* the screen's grid type, as last built */
#define LF_RECORD 0x4F8A50u       /* EncyclopediaRecord(entity): +0 name, +0x08 upper-case name, +0x24 designation */
#define LF_TEXT 0x17802E0u        /* (id): a text id's text */
#define LF_PICKER 0x18D8710u      /* BuildStratagemPicker(rcx, rdx, r8); rcx is its screen + 0xD2850 */
#define LF_PICKER_HELD 0x18D8AB5u /* call StratagemAvailable(account, type) in the walk */
#define LF_PICKER_BOUND 0x18D8AAAu /* the walk's bound: imm32 of mov edi, n (HellForge's stratagems grow it) */
#define LF_HELD 0x136FC20u
#define LF_ACCOUNT 0x347CEF8u
#define LF_PICKER_STATE 0x2818u   /* the picker's screen: 10 while the picker is up */
#define LF_POS 0x14476A0u        /* (widget, packed x, y): a widget's position, +y up */
#define LF_SIZE 0x1447160u       /* (widget, packed w, h): a widget's size */
#define LF_OPACITY 0x44u        /* a widget's opacity (0x1448AD0 sets it; 0 hidden) */
#define LF_CLEAR 0x18D27B0u       /* (column): every builder of the Armory's and picker's list column empties it first */
#define LF_ARMORY_H 0x23C7C64u    /* 738.0f: the column's visible height, Armory (the picker's, below, when +0x92FD1 is 1) */
#define LF_PICKER_H 0x23C7B98u    /* 528.0f */
#define LF_BAND_MAX 200.0f       /* units: the list keeps at least two rows of cards below the band */
#define LF_KIND 0x508E90u         /* EquipmentTypeRecord(entity): +0x80 the weapon's kind, the Armory's section */
#define LF_KIND_CHECK 0x18D7911u  /* the builder's cmp [rax+0x80], 0x20: 0x20 is no kind */
#define LF_KIND_TEXTS 0x18D7CCBu  /* the builder's heading: kind < 0x21 reads its text id from LF_KIND_TABLE */
#define LF_KIND_TABLE 0x32E25E0u
#define LF_GROUP_READ 0x18D8BFDu  /* the picker's read of a stratagem row's group, +0xB8 */
#define LF_GROUP_TEXTS 0x18D8D27u /* the picker's heading: group < 5 reads its text id from LF_GROUP_TABLE */
#define LF_GROUP_TABLE 0x32A8EC8u
#define LF_ROWS_MAX 1024u
#define LF_TYPES_MAX 4096u
#define LF_NAME 160u
#define LF_TEXT_MAX 96u
#define LF_TRAITS 256u
#define LF_LISTS 2u

static const uint8_t kLfBuildPin[] = {0x48, 0x89, 0x4C, 0x24, 0x08, 0x53, 0x55, 0x56, 0x57, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x60};
static const uint8_t kLfRowsPin[] = {0x48, 0x8B, 0x0D, 0x9B, 0xF2, 0xA4, 0x01, 0x46, 0x8B, 0xA4, 0xB9, 0x40, 0x1B, 0x00, 0x00, 0x45, 0x85, 0xE4};
static const uint8_t kLfTabPin[] = {0x48, 0x89, 0x5C, 0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x80, 0x79, 0x34, 0x00};
static const uint8_t kLfPickerPin[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20, 0x57};
static const uint8_t kLfWalkPin[] = {0xBF}; /* at LF_PICKER_BOUND - 1 */
static const uint8_t kLfWalkArgs[] = {0x8B, 0xD3, 0x49, 0x8B, 0xCE}; /* mov edx, ebx; mov rcx, r14: just before the held call */
static const uint8_t kLfPosPin[] = {0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x6C, 0x24, 0x20, 0x48, 0x89, 0x54, 0x24, 0x10, 0x56, 0x57, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x20, 0xF3, 0x0F, 0x10, 0x41, 0x04};
static const uint8_t kLfSizePin[] = {0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x6C, 0x24, 0x20, 0x48, 0x89, 0x54, 0x24, 0x10, 0x56, 0x57, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x20, 0xF3, 0x0F, 0x10, 0x41, 0x0C};
static const uint8_t kLfRecordPin[] = {0x48, 0x85, 0xC9, 0x74, 0x71, 0x48, 0x8B, 0x05};
static const uint8_t kLfTextPin[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x05};
static const uint8_t kLfClearPin[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20, 0x57, 0x48, 0x83, 0xEC, 0x70, 0x48, 0x8B, 0xF9};
/* The column's reads of its visible height: its fit (scroll bar and range), its rows' layout, its scroll into view and
 * its fade. Not its sizing (0x18D1076, 0x18D0F80): the band sizes the widgets itself. {site, length, picker} */
typedef struct LfHeightRead { uint32_t at; uint8_t length, picker; } LfHeightRead;
static const LfHeightRead kLfHeightReads[] = {{0x18D1C67u, 8, 1}, {0x18D1C71u, 8, 0}, {0x18D1E39u, 8, 1}, {0x18D1E43u, 8, 0}, {0x18D2D38u, 9, 1},
    {0x18D2D4Bu, 9, 0}, {0x18D4074u, 8, 1}, {0x18D407Eu, 8, 0}, {0x18D4592u, 8, 1}, {0x18D459Cu, 8, 0}};
static const uint8_t kLfKindPin[] = {0x48, 0x85, 0xC9, 0x74, 0x71, 0x48, 0x8B, 0x05, 0xFC, 0x30, 0xF6, 0x02};
static const uint8_t kLfKindCheckPin[] = {0x83, 0xB8, 0x80, 0x00, 0x00, 0x00, 0x20};
static const uint8_t kLfKindTextsPin[] = {0x83, 0xF8, 0x21, 0x73, 0x0C, 0x4C, 0x8D, 0x2D, 0x09, 0xA9, 0xA0, 0x01};
static const uint8_t kLfGroupReadPin[] = {0x8B, 0x83, 0xB8, 0x00, 0x00, 0x00};
static const uint8_t kLfGroupTextsPin[] = {0x83, 0xF8, 0x05, 0x73, 0x0F, 0x48, 0x8D, 0x15, 0xCD, 0x72, 0x72, 0xFE, 0x44, 0x8B, 0xA4, 0x82, 0xC8, 0x8E, 0x2A, 0x03};

typedef void (*LfBuildFn)(uintptr_t grid, uint32_t type);
typedef void (*LfTabFn)(uintptr_t screen, uint32_t tab);
typedef void (*LfPickerFn)(uint8_t* rcx, uint32_t rdx, void* r8);
typedef uint8_t (*LfHeldFn)(uintptr_t account, uint32_t type);
typedef uint8_t* (*LfRecordFn)(uint64_t entity);
typedef const char* (*LfTextFn)(uint32_t id);
typedef void (*LfSetFn)(uintptr_t widget, uint64_t packed);
typedef void (*LfClearFn)(uintptr_t column);

typedef struct LfRule { HfMod* mod; uint32_t list; HfListRule rule; void* user; } LfRule;
typedef struct LfBand { HfMod* mod; uint32_t list; float height; } LfBand;
/* A list's band as given: the grid it went to, the units moved, the heading's y after it, and its left, top, width */
typedef struct LfBandAt { uintptr_t root; float applied, headY, x, top, w; } LfBandAt;
typedef struct LfFilter { HfMod* mod; uint32_t list; char text[LF_TEXT_MAX]; } LfFilter;

static SRWLOCK g_lfLock = SRWLOCK_INIT;
static LfRule g_lfRules[32];
static LfFilter g_lfFilters[32];
static LfBand g_lfBands[16];
static LfBandAt g_lfBandAt[LF_LISTS];
static uint32_t g_lfRuleCount, g_lfFilterCount, g_lfBandCount;
static int g_lfWidgets; /* the widget setters match: bands can be given */
static volatile float* g_lfHeights; /* [0] Armory, [1] picker: the visible heights the column reads; NULL: the game's */
static LfClearFn g_lfClear;
static int g_lfWeaponBuild;        /* inside the weapon grid's build: its column is the one with the band */
static uint32_t g_lfOpened[LF_LISTS]; /* the game's own builds of each list (HfListState.opened) */
static int g_lfOwnBuild;              /* inside LfRebuildNow: a build HellForge asked for */
static HfListState g_lfState[LF_LISTS] = {{sizeof(HfListState)}, {sizeof(HfListState)}}; /* open follows the screen */
static volatile LONG g_lfRebuild[LF_LISTS], g_lfTicking;
static int g_lfArmory, g_lfPicker; /* 0 not yet, 1 on, -1 off (modloader.log says why) */
static int g_lfKinds, g_lfGroups;  /* the section reads match: rows get their category */
static ULONGLONG g_lfWaitSince;
static uintptr_t g_lfGame;
static HfMod g_lfMod;
static LfBuildFn g_lfBuild;
static LfPickerFn g_lfPickerBuild;
static uint8_t* g_lfPickerArgs[2];
static uint32_t g_lfPickerRdx;
static uint8_t g_lfHidden[LF_TYPES_MAX / 8]; /* stratagem types the picker's current build hides */
static int g_lfBuilding;                     /* inside a picker build: the held call consults g_lfHidden */

static void LfLower(char* s) { for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a'); }

static const char* LfText(uint32_t id)
{
    const char* own = id ? ModTextOf(id) : NULL;
    if (own != NULL || id == 0) return own;
    const char* text = ((LfTextFn)(g_lfGame + LF_TEXT))(id);
    return text != NULL && text[0] && strncmp(text, "#ID[", 4) != 0 ? text : NULL;
}

/* "<designation> <name>" as the Armory card shows it; some vanilla melee have only the upper-case name. */
static void LfWeaponName(uint64_t entity, char* out, size_t size)
{
    uint32_t ids[3] = {0};
    uint8_t* record = entity ? ((LfRecordFn)(g_lfGame + LF_RECORD))(entity) : NULL;
    out[0] = 0;
    if (record == NULL || SafeRead((uintptr_t)record, &ids[0], 4) || SafeRead((uintptr_t)record + 0x08, &ids[1], 4) ||
        SafeRead((uintptr_t)record + 0x24, &ids[2], 4)) return;
    const char* name = LfText(ids[0]);
    if (name == NULL) name = LfText(ids[1]);
    const char* designation = LfText(ids[2]);
    snprintf(out, size, "%s%s%s", designation ? designation : "", designation && name ? " " : "", name ? name : "");
}

static void LfStratagemName(uint32_t type, char* out, size_t size)
{
    uintptr_t table = g_stratagemTable ? (uintptr_t)g_stratagemTable : g_lfGame + STR_TABLE, row = 0;
    uint32_t ids[2] = {0};
    out[0] = 0;
    if (SafeRead(table + (uintptr_t)type * 8u, &row, 8) || !row || SafeRead(row + 0x2C, &ids[0], 4) || SafeRead(row + 0x28, &ids[1], 4)) return;
    const char* name = LfText(ids[0]);
    if (name == NULL) name = LfText(ids[1]);
    if (name != NULL) snprintf(out, size, "%s", name);
}

/* The heading a row is listed under, as the list's own heading shows it: the Armory's by the weapon's kind, the
 * picker's by the stratagem's group. "" when the reads did not match or the row has none. */
static void LfWeaponCategory(uint64_t entity, char* out, size_t size)
{
    uint8_t* record = g_lfKinds && entity ? ((LfRecordFn)(g_lfGame + LF_KIND))(entity) : NULL;
    uint32_t kind = 0x20, id = 0;
    out[0] = 0;
    if (record == NULL || SafeRead((uintptr_t)record + 0x80, &kind, 4) || kind > 0x20 || SafeRead(g_lfGame + LF_KIND_TABLE + kind * 4u, &id, 4)) return;
    const char* text = LfText(id);
    if (text != NULL) snprintf(out, size, "%s", text);
}

static void LfStratagemCategory(uint32_t type, char* out, size_t size)
{
    /* a mod's heading (add_stratagem_category) when the type is listed under one; else the game's group */
    int32_t order = 0;
    const uint32_t handle = StratagemSelectionPlace(type, &order);
    if (handle) { snprintf(out, size, "%s", g_stratagemCategories[handle - 1].title); return; }
    uintptr_t table = g_stratagemTable ? (uintptr_t)g_stratagemTable : g_lfGame + STR_TABLE, row = 0;
    uint32_t group = 0, id = 0;
    out[0] = 0;
    if (!g_lfGroups || SafeRead(table + (uintptr_t)type * 8u, &row, 8) || !row || SafeRead(row + 0xB8, &group, 4) || group >= 5 ||
        SafeRead(g_lfGame + LF_GROUP_TABLE + group * 4u, &id, 4)) return;
    const char* text = LfText(id);
    if (text != NULL) snprintf(out, size, "%s", text);
}

static void LfAppend(char* out, size_t size, const char* text)
{
    const size_t at = strlen(out);
    if (text != NULL && text[0] && at + 3 < size) snprintf(out + at, size - at, "%s%s", at ? ", " : "", text);
}

/* The trait lines a weapon's card shows: its loadout record's traits_1..5, as the game has them now. */
static void LfWeaponTraits(uint64_t entity, char* out, size_t size)
{
    static const char* const slots[] = {"traits_1", "traits_2", "traits_3", "traits_4", "traits_5"};
    const SdkComponent* c = SdkComponentNamed("LoadoutEntry");
    out[0] = 0;
    if (c == NULL || entity == 0) return;
    AcquireSRWLockExclusive(&g_fieldsLock);
    for (size_t i = 0; i < sizeof slots / sizeof slots[0]; i++)
    {
        const SdkField* f = SdkFieldNamed(c, slots[i]);
        uint8_t* at = f != NULL ? LiveAddress(&g_lfMod, entity, c, f) : NULL;
        uint32_t id = 0;
        if (at != NULL && !SafeRead((uintptr_t)at, &id, 4) && id) LfAppend(out, size, LfText(id));
    }
    ReleaseSRWLockExclusive(&g_fieldsLock);
}

/* A stratagem card's ITEM TRAITS lines: a mod's (set_stratagem_traits), else the game's set tags in its order. */
static void LfStratagemTraits(uint32_t type, char* out, size_t size)
{
    out[0] = 0;
    if (g_itState != 1) return;
    uint32_t ids[IT_TAGS] = {0}, count = 0;
    AcquireSRWLockShared(&g_itLock);
    const ItCard* card = ItCardOf(type);
    if (card != NULL) count = card->count, memcpy(ids, card->ids, card->count * 4u);
    ReleaseSRWLockShared(&g_itLock);
    if (card == NULL)
        for (uint32_t i = 0; i < IT_TAGS; i++)
            if (((ItFlagFn)(g_itGame + IT_FLAG))(type, i) && !SafeRead(g_itGame + IT_TAG_IDS + i * 4u, &ids[count], 4)) count++;
    for (uint32_t i = 0; i < count; i++) LfAppend(out, size, ItTextOfId(ids[i]));
}

/* "#text" searches the category, "@text" the traits, anything else the name. text and the row's fields are lower case. */
static int LfMatches(const char* text, const char* name, const char* category, const char* traits)
{
    if (text[0] == '#') return strstr(category, text + 1) != NULL;
    if (text[0] == '@') return strstr(traits, text + 1) != NULL;
    return strstr(name, text) != NULL;
}

/* Whether a row is kept by every rule, and (*match) whether every filter matches it. Under g_lfLock, shared. */
static int LfKeep(const HfListRow* row, int* match)
{
    for (uint32_t i = 0; i < g_lfRuleCount; i++)
        if (g_lfRules[i].list == row->list && !g_lfRules[i].rule(g_lfRules[i].mod, row, g_lfRules[i].user)) return 0;
    char name[LF_NAME], category[LF_NAME], traits[LF_TRAITS];
    snprintf(name, sizeof name, "%s", row->name);
    snprintf(category, sizeof category, "%s", row->category);
    snprintf(traits, sizeof traits, "%s", row->traits);
    LfLower(name), LfLower(category), LfLower(traits);
    *match = 1;
    for (uint32_t i = 0; i < g_lfFilterCount && *match; i++)
        if (g_lfFilters[i].list == row->list && g_lfFilters[i].text[0])
            *match = LfMatches(g_lfFilters[i].text, name, category, traits);
    return 1;
}

static int LfUsed(uint32_t list)
{
    for (uint32_t i = 0; i < g_lfRuleCount; i++) if (g_lfRules[i].list == list) return 1;
    for (uint32_t i = 0; i < g_lfFilterCount; i++) if (g_lfFilters[i].list == list && g_lfFilters[i].text[0]) return 1;
    for (uint32_t i = 0; i < g_lfBandCount; i++) if (g_lfBands[i].list == list && g_lfBands[i].height > 0) return 1; /* list_state places it */
    return 0;
}

/* ---- the Armory's weapon grids ---- */

static void LfGridBuildRows(uintptr_t grid, uint32_t type)
{
    static uintptr_t at[LF_ROWS_MAX];
    static uint32_t item[LF_ROWS_MAX];
    static uint8_t keep[LF_ROWS_MAX], match[LF_ROWS_MAX];
    uintptr_t table = 0;
    uint32_t count = 0, first = 0, n = 0, total = 0, matched = 0;
    AcquireSRWLockShared(&g_lfLock);
    if (!LfUsed(HF_LIST_ARMORY_WEAPONS) || SafeRead(g_lfGame + LF_ARMORY, &table, 8) || !table || type >= 8 ||
        SafeRead(table + 0x1B40u + type * 4u, &count, 4) || SafeRead(table + 0x1B24u + type * 4u, &first, 4) || count > LF_ROWS_MAX)
    {
        ReleaseSRWLockShared(&g_lfLock);
        g_lfBuild(grid, type);
        return;
    }
    const uintptr_t rows = table + (uintptr_t)first * 0x20u;
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t id = 0;
        uint64_t entity = 0;
        char name[LF_NAME], category[LF_NAME], traits[LF_TRAITS];
        if (SafeRead(rows + i * 0x20u, &id, 4) || !id || SafeRead(rows + i * 0x20u + 8u, &entity, 8)) continue; /* listed by no one */
        LfWeaponName(entity, name, sizeof name);
        LfWeaponCategory(entity, category, sizeof category);
        LfWeaponTraits(entity, traits, sizeof traits);
        HfListRow row = {sizeof row, HF_LIST_ARMORY_WEAPONS, type, id, entity, name, category, traits};
        int m = 0;
        at[n] = rows + i * 0x20u, item[n] = id, keep[n] = (uint8_t)LfKeep(&row, &m), match[n] = (uint8_t)m;
        total += keep[n], matched += keep[n] && m;
        n++;
    }
    ReleaseSRWLockShared(&g_lfLock);
    /* a filter that matches nothing leaves the rules' list: an empty grid is not a state the page knows */
    const int filter = matched > 0;
    for (uint32_t k = 0; k < n; k++) if (!keep[k] || (filter && !match[k])) *(volatile uint32_t*)at[k] = 0;
    g_lfBuild(grid, type);
    for (uint32_t k = 0; k < n; k++) *(volatile uint32_t*)at[k] = item[k];
    g_lfState[0] = (HfListState){sizeof(HfListState), 1, type, total, matched, filter ? matched : total};
}

static uintptr_t LfArmoryScreen(void)
{
    uintptr_t object = 0, screen = 0;
    uint32_t state = 0;
    if (SafeRead(g_lfGame + LF_MENU, &object, 8) || !object || SafeRead(object, &state, 4) || state != 2) return 0;
    return SafeRead(object + 0x60, &screen, 8) == 0 ? screen : 0;
}

/* The weapon page's tab whose type the grid was last built as; -1 when its list is not up. */
static int LfArmoryTab(uintptr_t screen)
{
    uint32_t mode = 1, built = 0, type = 0;
    uint8_t tabs = 0;
    if (!screen || SafeRead(screen + 0x30, &mode, 4) || mode != 0 || SafeRead(screen + 0x34, &tabs, 1) || !tabs ||
        SafeRead(screen + LF_GRID_TYPE, &built, 4)) return -1;
    for (int i = 0; i < 4; i++)
        if (!SafeRead(screen + LF_TAB_TYPES + (uintptr_t)i * 0x3010u, &type, 4) && type == built) return i;
    return -1;
}

/* ---- the band at the top of a list ---- */
/* The grids' root widget: .1 the list column (.0 its scroll track, .1 its viewport), .2 the category heading bar,
 * whose position is its bottom edge. The game places both only when it builds the page, so a band given once stays
 * until the next build, which LfBandApply notices by the heading's y. */

static uintptr_t LfChild(uintptr_t widget, int n)
{
    uintptr_t c = 0;
    if (!widget || SafeRead(widget + 0xE0, &c, 8)) return 0;
    for (; c && n > 0; n--) if (SafeRead(c + 0xE8, &c, 8)) return 0;
    return c;
}

static uint64_t LfPack(float a, float b)
{
    uint32_t x, y;
    memcpy(&x, &a, 4), memcpy(&y, &b, 4);
    return (uint64_t)y << 32 | x;
}

static void LfShift(uintptr_t widget, float dy, int move)
{
    float r[4];
    if (!widget || SafeRead(widget + 4, r, 16)) return;
    if (move) ((LfSetFn)(g_lfGame + LF_POS))(widget, LfPack(r[0], r[1] - dy));
    else ((LfSetFn)(g_lfGame + LF_SIZE))(widget, LfPack(r[2], r[3] - dy));
}

static float LfBandTotal(uint32_t list)
{
    float total = 0;
    AcquireSRWLockShared(&g_lfLock);
    for (uint32_t i = 0; i < g_lfBandCount; i++) if (g_lfBands[i].list == list) total += g_lfBands[i].height;
    ReleaseSRWLockShared(&g_lfLock);
    return total > LF_BAND_MAX ? LF_BAND_MAX : total;
}

/* Moves the heading and the list down by the bands of the list, and the list's bottom stays. Game thread. */
static void LfBandApply(uint32_t list, uintptr_t root)
{
    LfBandAt* at = &g_lfBandAt[list - 1];
    const uintptr_t column = LfChild(root, 1), heading = LfChild(root, 2), track = LfChild(column, 0);
    float h[4];
    if (!g_lfWidgets || !column || !heading || SafeRead(heading + 4, h, 16)) return;
    /* built again: the game put the heading back. The same heading still where the band put it (back from CUSTOMIZE,
     * which hides the list without a build) keeps its band, or it would be given twice */
    if (at->applied != 0 && (root != at->root || h[1] != at->headY)) at->applied = 0;
    at->root = root;
    const float dy = LfBandTotal(list) - at->applied;
    if (dy != 0)
    {
        LfShift(heading, dy, 1), LfShift(column, dy, 1);
        LfShift(column, dy, 0), LfShift(track, dy, 0), LfShift(LfChild(track, 0), dy, 0), LfShift(LfChild(column, 1), dy, 0);
        at->applied += dy;
        SafeRead(heading + 4, h, 16);
    }
    at->headY = h[1], at->x = h[0], at->w = h[2], at->top = -h[1] - h[3] - at->applied;
}

/* Whether a grid is on screen: its screen keeps it, and its state, while it is hidden (the loadout card under the
 * picker), and hides it by its root widget's opacity, which the grid starts at 0. */
static int LfShown(uintptr_t root)
{
    float opacity = 0;
    return root && !SafeRead(root + LF_OPACITY, &opacity, 4) && opacity > 0.01f;
}

/* ---- the stratagem picker ---- */

static int LfHiddenType(uint32_t type) { return type < LF_TYPES_MAX && (g_lfHidden[type / 8] >> (type % 8) & 1); }

/* The walk's held call: no for a type this build hides. */
static uint8_t LfHeld(uintptr_t account, uint32_t type)
{
    uint8_t held = ((LfHeldFn)(g_lfGame + LF_HELD))(account, type);
    return held && g_lfBuilding && LfHiddenType(type) ? 0 : held;
}

/* Which held types this build hides, decided before the walk so a filter that matches nothing can step aside. */
/* The column's visible height is a constant the game reads in its fit, layout and scrolling; a band takes its top, so
 * the column reads a height less the band instead, set by each build: the Armory's other pages share the column. */
static void LfClear(uintptr_t column)
{
    if (g_lfHeights != NULL)
    {
        g_lfHeights[0] = 738.0f - (g_lfWeaponBuild && g_lfWidgets ? LfBandTotal(HF_LIST_ARMORY_WEAPONS) : 0);
        g_lfHeights[1] = 528.0f - (g_lfWidgets ? LfBandTotal(HF_LIST_STRATAGEMS) : 0);
    }
    g_lfClear(column);
}

static void LfGridBuild(uintptr_t grid, uint32_t type)
{
    if (!g_lfOwnBuild) g_lfOpened[0]++;
    g_lfWeaponBuild = 1;
    LfGridBuildRows(grid, type);
    g_lfWeaponBuild = 0;
}

/* The heights the column reads pointed at HellForge's, near game.dll: each read is a movss from a rip-relative address. */
static const char* LfHeightsInstall(uintptr_t game)
{
    float values[2] = {0};
    if (SafeRead(game + LF_ARMORY_H, &values[0], 4) || SafeRead(game + LF_PICKER_H, &values[1], 4) || values[0] != 738.0f || values[1] != 528.0f)
        return "its heights are not the reviewed ones";
    for (size_t i = 0; i < sizeof kLfHeightReads / sizeof kLfHeightReads[0]; i++)
    {
        const LfHeightRead* r = &kLfHeightReads[i];
        int32_t disp = 0;
        uint8_t op[2] = {0}, modrm = 0; /* movss xmm, [rip + disp32]: 0F 10, modrm 00 reg 101, then the disp */
        if (SafeRead(game + r->at + r->length - 4, &disp, 4) || SafeRead(game + r->at + r->length - 7, op, 2) || SafeRead(game + r->at + r->length - 5, &modrm, 1) || op[0] != 0x0F || op[1] != 0x10 || (modrm & 0xC7) != 0x05 ||
            game + r->at + r->length + disp != game + (r->picker ? LF_PICKER_H : LF_ARMORY_H))
            return "its height reads are not the reviewed ones";
    }
    if (!PinMatches(game + LF_CLEAR, kLfClearPin, sizeof kLfClearPin)) return "its column's clear is not the reviewed one";
    float* heights = (float*)AllocateNearGame(game, 64);
    if (heights == NULL) return "no memory within 2 GB of game.dll";
    heights[0] = values[0], heights[1] = values[1];
    if (LoaderHook("lists: the list column's clear", game + LF_CLEAR, (void*)LfClear, (void**)&g_lfClear)) return "its clear could not be hooked";
    for (size_t i = 0; i < sizeof kLfHeightReads / sizeof kLfHeightReads[0]; i++)
    {
        const LfHeightRead* r = &kLfHeightReads[i];
        uint8_t* field = (uint8_t*)(game + r->at + r->length - 4);
        const int32_t disp = (int32_t)((intptr_t)&heights[r->picker] - (intptr_t)(game + r->at + r->length));
        DWORD was;
        VirtualProtect(field, 4, PAGE_EXECUTE_READWRITE, &was);
        memcpy(field, &disp, 4);
        VirtualProtect(field, 4, was, &was);
    }
    FlushInstructionCache(GetCurrentProcess(), (void*)(game + LF_CLEAR - 0x2000), 0x4000);
    g_lfHeights = heights;
    return NULL;
}

static void LfPickerDecide(void)
{
    uintptr_t account = 0;
    uint32_t bound = 0, total = 0, matched = 0;
    memset(g_lfHidden, 0, sizeof g_lfHidden);
    if (SafeRead(g_lfGame + LF_ACCOUNT, &account, 8) || !account || SafeRead(g_lfGame + LF_PICKER_BOUND, &bound, 4)) return;
    if (bound > LF_TYPES_MAX) bound = LF_TYPES_MAX;
    static uint8_t miss[LF_TYPES_MAX / 8];
    memset(miss, 0, sizeof miss);
    AcquireSRWLockShared(&g_lfLock);
    for (uint32_t type = 1; type < bound; type++)
    {
        if (!((LfHeldFn)(g_lfGame + LF_HELD))(account, type)) continue;
        char name[LF_NAME], category[LF_NAME], traits[LF_TRAITS];
        LfStratagemName(type, name, sizeof name);
        LfStratagemCategory(type, category, sizeof category);
        LfStratagemTraits(type, traits, sizeof traits);
        HfListRow row = {sizeof row, HF_LIST_STRATAGEMS, type, 0, 0, name, category, traits};
        int m = 0;
        if (!LfKeep(&row, &m)) { g_lfHidden[type / 8] |= (uint8_t)(1u << (type % 8)); continue; }
        total++;
        if (m) matched++; else miss[type / 8] |= (uint8_t)(1u << (type % 8));
    }
    ReleaseSRWLockShared(&g_lfLock);
    if (matched) for (size_t i = 0; i < sizeof miss; i++) g_lfHidden[i] |= miss[i];
    g_lfState[1] = (HfListState){sizeof(HfListState), 1, 0, total, matched, matched ? matched : total};
}

static void LfPickerBuild(uint8_t* rcx, uint32_t rdx, void* r8)
{
    if (!g_lfOwnBuild) g_lfOpened[1]++;
    g_lfPickerArgs[0] = rcx, g_lfPickerArgs[1] = (uint8_t*)r8, g_lfPickerRdx = rdx;
    AcquireSRWLockShared(&g_lfLock);
    const int used = LfUsed(HF_LIST_STRATAGEMS);
    ReleaseSRWLockShared(&g_lfLock);
    if (used) LfPickerDecide();
    else memset(g_lfHidden, 0, sizeof g_lfHidden), g_lfState[1] = (HfListState){sizeof(HfListState), 1, 0, 0, 0, 0};
    g_lfBuilding = used;
    g_lfPickerBuild(rcx, rdx, r8);
    g_lfBuilding = 0;
}

static int LfPickerOpen(void)
{
    uint32_t state = 0;
    return g_lfPickerArgs[0] != NULL && !SafeRead((uintptr_t)g_lfPickerArgs[0] - 0xD2850u + LF_PICKER_STATE, &state, 4) && state == 10;
}

/* ---- install and the game-thread update ---- */

static const char* LfArmoryInstall(uintptr_t game)
{
    g_lfWidgets = PinMatches(game + LF_POS, kLfPosPin, sizeof kLfPosPin) && PinMatches(game + LF_SIZE, kLfSizePin, sizeof kLfSizePin);
    const char* heights = g_lfWidgets ? LfHeightsInstall(game) : "the widget setters are not the reviewed ones";
    if (heights != NULL) Note("lists: bands keep the list's full height (%s): its bottom rows can stay below the edge", heights);
    if (!PinMatches(game + LF_GRID_BUILD, kLfBuildPin, sizeof kLfBuildPin) || !PinMatches(game + LF_ROWS_READ, kLfRowsPin, sizeof kLfRowsPin) ||
        !PinMatches(game + LF_TAB_SELECT, kLfTabPin, sizeof kLfTabPin) || !PinMatches(game + LF_RECORD, kLfRecordPin, sizeof kLfRecordPin) ||
        !PinMatches(game + LF_TEXT, kLfTextPin, sizeof kLfTextPin))
        return "the Armory grid's code is not the reviewed one";
    g_lfKinds = PinMatches(game + LF_KIND, kLfKindPin, sizeof kLfKindPin) && PinMatches(game + LF_KIND_CHECK, kLfKindCheckPin, sizeof kLfKindCheckPin) &&
                PinMatches(game + LF_KIND_TEXTS, kLfKindTextsPin, sizeof kLfKindTextsPin);
    return LoaderHook("lists: the Armory weapon grid", game + LF_GRID_BUILD, (void*)LfGridBuild, (void**)&g_lfBuild) ? "its builder could not be hooked" : NULL;
}

static const char* LfPickerInstall(uintptr_t game)
{
    if (!PinMatches(game + LF_PICKER, kLfPickerPin, sizeof kLfPickerPin) || !PinMatches(game + LF_PICKER_BOUND - 1, kLfWalkPin, sizeof kLfWalkPin) ||
        !PinMatches(game + LF_PICKER_HELD - sizeof kLfWalkArgs, kLfWalkArgs, sizeof kLfWalkArgs) || !ItCallsTo(game + LF_PICKER_HELD, game + LF_HELD))
        return "the picker's walk is not the reviewed one";
    g_lfGroups = PinMatches(game + LF_GROUP_READ, kLfGroupReadPin, sizeof kLfGroupReadPin) && PinMatches(game + LF_GROUP_TEXTS, kLfGroupTextsPin, sizeof kLfGroupTextsPin);
    uint8_t* stub = AllocateNearGame(game, 64);
    if (stub == NULL) return "no memory within 2 GB of game.dll";
    ItStub(stub, (void*)LfHeld, 0);
    DWORD was;
    VirtualProtect(stub, 64, PAGE_EXECUTE_READ, &was);
    FlushInstructionCache(GetCurrentProcess(), stub, 64);
    if (LoaderHook("lists: the stratagem picker", game + LF_PICKER, (void*)LfPickerBuild, (void**)&g_lfPickerBuild)) return "its builder could not be hooked";
    /* a stub that is never reached is harmless, and game code may hold it: it is not freed */
    return ItPoint(game + LF_PICKER_HELD, (uintptr_t)stub) ? "its held call could not be pointed at HellForge" : NULL;
}

/* The asked rebuilds of the lists that are up, on the game thread, outside any build. */
static void LfRebuildNow(void)
{
    g_lfOwnBuild = 1;
    if (InterlockedExchange(&g_lfRebuild[0], 0) && g_lfArmory == 1)
    {
        const uintptr_t screen = LfArmoryScreen();
        const int tab = LfArmoryTab(screen);
        if (tab >= 0) ((LfTabFn)(g_lfGame + LF_TAB_SELECT))(screen, (uint32_t)tab);
    }
    if (InterlockedExchange(&g_lfRebuild[1], 0) && g_lfPicker == 1 && LfPickerOpen())
        LfPickerBuild(g_lfPickerArgs[0], g_lfPickerRdx, g_lfPickerArgs[1]);
    g_lfOwnBuild = 0;
}

static void LfNoteInstall(const char* armory, const char* picker)
{
    Note("lists: Armory weapon grids %s%s; stratagem picker %s%s; categories: weapons %s, stratagems %s", armory ? "off: " : "on", armory ? armory : "",
         picker ? "off: " : "on", picker ? picker : "", g_lfKinds ? "on" : "off", g_lfGroups ? "on" : "off");
}

/* game.dll's code is decoded a while after it loads: tried each update for a minute. 1 when tried. */
static int LfInstall(void)
{
    uintptr_t game = ApiModuleBase("game.dll", NULL);
    const char* why = strcmp(g_build, "25480438") ? "unsupported game build" : game == 0 ? "game.dll is not loaded" : NULL;
    if (why == NULL && !PinMatches(game + LF_GRID_BUILD, kLfBuildPin, sizeof kLfBuildPin))
    {
        if (g_lfWaitSince == 0) g_lfWaitSince = GetTickCount64();
        if (GetTickCount64() - g_lfWaitSince < 60000) return 0;
    }
    g_lfGame = game;
    const char* armory = why ? why : LfArmoryInstall(game);
    const char* picker = why ? why : LfPickerInstall(game);
    g_lfArmory = armory ? -1 : 1, g_lfPicker = picker ? -1 : 1;
    LfNoteInstall(armory, picker);
    return 1;
}

static void LfTick(HfMod* mod, float dt, void* user)
{
    (void)mod, (void)dt, (void)user;
    if (!g_fired[HF_EVENT_GAME_LOADED] || ((g_lfArmory == 0 || g_lfPicker == 0) && !LfInstall())) return;
    /* open follows the screen each update both ways: a tab change can pass a frame or two without a list */
    const uintptr_t screen = LfArmoryScreen();
    const int armoryPage = g_lfArmory == 1 && LfArmoryTab(screen) >= 0, pickerPage = g_lfPicker == 1 && LfPickerOpen();
    const int armory = armoryPage && LfShown(screen + LF_ARMORY_GRID), picker = pickerPage && LfShown((uintptr_t)g_lfPickerArgs[0]);
    static int seen = -1;
    const int now = armoryPage | armory << 1 | pickerPage << 2 | picker << 3;
    if (now != seen) Note("lists: Armory page %d shown %d, picker page %d shown %d", armoryPage, armory, pickerPage, picker), seen = now;
    g_lfState[0].open = (uint32_t)armory, g_lfState[1].open = (uint32_t)picker;
    LfRebuildNow();
    if (armory) LfBandApply(HF_LIST_ARMORY_WEAPONS, screen + LF_ARMORY_GRID);
    if (picker) LfBandApply(HF_LIST_STRATAGEMS, (uintptr_t)g_lfPickerArgs[0]);
}

static void LfStartTick(void)
{
    if (InterlockedExchange(&g_lfTicking, 1) != 0) return;
    g_lfMod.api = &g_api;
    snprintf(g_lfMod.id, sizeof g_lfMod.id, "hellforge.lists");
    g_lfMod.log = g_log;
    g_lfMod.loaded = 1;
    ApiOnGameThread(&g_lfMod, LfTick, NULL);
}

/* ---- the API ---- */

static int LfListOk(HfMod* mod, uint32_t list, const char* call)
{
    if (mod != NULL && (list == HF_LIST_ARMORY_WEAPONS || list == HF_LIST_STRATAGEMS)) return 1;
    ApiLog(mod, "%s: refused: it needs an HF_LIST_ list", call);
    return 0;
}

/* SDK 38, add_list_rule: every build of the list asks rule of each row. 0 done, 1 refused (logged). */
static int ApiAddListRule(HfMod* mod, uint32_t list, HfListRule rule, void* user)
{
    if (!LfListOk(mod, list, "add_list_rule")) return 1;
    if (rule == NULL) { ApiLog(mod, "add_list_rule: refused: no rule"); return 1; }
    AcquireSRWLockExclusive(&g_lfLock);
    const int room = g_lfRuleCount < sizeof g_lfRules / sizeof g_lfRules[0];
    if (room) g_lfRules[g_lfRuleCount++] = (LfRule){mod, list, rule, user};
    ReleaseSRWLockExclusive(&g_lfLock);
    if (!room) { ApiLog(mod, "add_list_rule: no room for more rules"); return 1; }
    ApiLog(mod, "add_list_rule: a rule for the %s", list == HF_LIST_STRATAGEMS ? "stratagem picker" : "Armory weapon grids");
    LfStartTick();
    InterlockedExchange(&g_lfRebuild[list - 1], 1);
    return 0;
}

/* SDK 38, set_list_filter: text every listed row's name must contain, any case; NULL or "" takes the mod's away.
 * SDK 44: "#text" is text the row's category must contain instead; SDK 50: "@text", its traits. */
static int ApiSetListFilter(HfMod* mod, uint32_t list, const char* text)
{
    if (!LfListOk(mod, list, "set_list_filter")) return 1;
    char lower[LF_TEXT_MAX];
    snprintf(lower, sizeof lower, "%s", text ? text : "");
    LfLower(lower);
    AcquireSRWLockExclusive(&g_lfLock);
    LfFilter* f = NULL;
    for (uint32_t i = 0; i < g_lfFilterCount && f == NULL; i++) if (g_lfFilters[i].mod == mod && g_lfFilters[i].list == list) f = &g_lfFilters[i];
    if (f == NULL && g_lfFilterCount < sizeof g_lfFilters / sizeof g_lfFilters[0]) f = &g_lfFilters[g_lfFilterCount++], f->mod = mod, f->list = list;
    const int changed = f != NULL && strcmp(f->text, lower) != 0;
    if (f != NULL) memcpy(f->text, lower, sizeof lower);
    ReleaseSRWLockExclusive(&g_lfLock);
    if (f == NULL) { ApiLog(mod, "set_list_filter: no room for more filters"); return 1; }
    LfStartTick();
    if (changed) InterlockedExchange(&g_lfRebuild[list - 1], 1);
    return 0;
}

/* SDK 38, rebuild_list: the list built again on the next update, when it is up. 0 asked, 1 refused or not up. */
static int ApiRebuildList(HfMod* mod, uint32_t list)
{
    if (!LfListOk(mod, list, "rebuild_list")) return 1;
    LfStartTick();
    InterlockedExchange(&g_lfRebuild[list - 1], 1);
    return g_lfState[list - 1].open ? 0 : 1;
}

/* The calling mod's slice of a list's band, below the slices of the mods that asked before it. */
static void LfOwnBand(HfMod* mod, uint32_t list, HfListState* out)
{
    const LfBandAt* at = &g_lfBandAt[list - 1];
    float above = 0, own = 0;
    AcquireSRWLockShared(&g_lfLock);
    for (uint32_t i = 0; i < g_lfBandCount && own == 0; i++)
        if (g_lfBands[i].list == list) *(g_lfBands[i].mod == mod ? &own : &above) += g_lfBands[i].height;
    ReleaseSRWLockShared(&g_lfLock);
    if (own <= 0 || !out->open || above >= at->applied) return;
    out->band_x = at->x, out->band_y = at->top + above, out->band_w = at->w;
    out->band_h = own < at->applied - above ? own : at->applied - above;
}

/* SDK 38, list_state: what the list showed at its last build. 1 filled, 0 not (no build seen, or a bad call). */
static uint32_t ApiListState(HfMod* mod, uint32_t list, HfListState* state)
{
    const uint32_t size = state ? state->size : 0;
    if (size < offsetof(HfListState, band_x) || (list != HF_LIST_ARMORY_WEAPONS && list != HF_LIST_STRATAGEMS) || g_lfState[list - 1].size == 0) return 0;
    HfListState out = g_lfState[list - 1];
    LfOwnBand(mod, list, &out);
    out.opened = g_lfOpened[list - 1];
    memcpy(state, &out, size < sizeof out ? size : sizeof out);
    state->size = size;
    return 1;
}

/* SDK 40, set_list_band: room of height units at the top of the list for the mod to draw in; 0 takes it away. */
static int ApiSetListBand(HfMod* mod, uint32_t list, float height)
{
    if (!LfListOk(mod, list, "set_list_band")) return 1;
    if (!(height >= 0 && height <= LF_BAND_MAX)) { ApiLog(mod, "set_list_band: refused: height %.0f is not 0 .. %.0f", height, LF_BAND_MAX); return 1; }
    AcquireSRWLockExclusive(&g_lfLock);
    LfBand* b = NULL;
    for (uint32_t i = 0; i < g_lfBandCount && b == NULL; i++) if (g_lfBands[i].mod == mod && g_lfBands[i].list == list) b = &g_lfBands[i];
    if (b == NULL && g_lfBandCount < sizeof g_lfBands / sizeof g_lfBands[0]) b = &g_lfBands[g_lfBandCount++], b->mod = mod, b->list = list;
    const int first = b != NULL && b->height == 0 && height > 0;
    if (b != NULL) b->height = height;
    ReleaseSRWLockExclusive(&g_lfLock);
    if (b == NULL) { ApiLog(mod, "set_list_band: no room for more bands"); return 1; }
    LfStartTick();
    if (first && !g_lfState[list - 1].open) InterlockedExchange(&g_lfRebuild[list - 1], 1); /* so list_state has a build */
    return 0;
}

/* The table's entries from SDK 38 on (ApiInit). */
#define LF_API ApiAddListRule, ApiSetListFilter, ApiRebuildList, ApiListState, ApiSetListBand
