/* SDK 26 add_armory_tab and SDK 27 add_menu_tab: mods' labels on the game's own tab bars, for game.dll build 25480438.
 * Each bar is one widget (labels +0xDFE8, count +0xE068, selected +0xE06C) inside a menu screen; the screen reacts to
 * the bar's tab change with its own code. Where that code would use a mod's index, its select is hooked and returns
 * before it; where it ignores an index past its own labels, nothing is hooked. Included at the end of world_calls.c
 * (its menu reads and pins). Layout: docs/internals-world.md "The game's tab bars". */

#define AT_SET 0x17AAC50u  /* (bar, ids, count): the bar's labels, up to 8 text ids (detoured by the Megapack: not hooked) */
#define AT_PICK 0x17ABFC0u /* (bar, index, u8, u8): the bar's selected label */
#define AT_TITLE 0x17AB370u /* (bar, text): the title over the bar (the page's name), as the page open sets it */
#define AT_BAR_IDS 0xDFE8u
#define AT_BAR_COUNT 0xE068u
#define AT_BAR_INDEX 0xE06Cu
#define AT_LABELS 8u
#define AT_TABS 48u

#define AT_ARMORY_SELECT 0x14570B0u  /* (screen, index): the Armory's tab select; a mod's index never reaches it */
#define AT_ARMORY_OPEN 0x1457350u    /* (screen, open): opens or closes a category's page and sets the bar's labels */
#define AT_ARMORY_INPUT 0x1457A10u   /* (screen, input, ...): the Armory's input; sets the weapon labels after the grid */
#define AT_LOADOUT_SELECT 0x1470D90u /* (screen, index, u8): the Loadout's tab select; a mod's index never reaches it */

/* The screens with a bar: the menu state that shows it, where the menu object (WC_MENU) keeps the screen, and the bar
 * inside it (screen + sub + bar). */
typedef struct AtBar { uint32_t menu, screen, sub, bar; } AtBar;
enum { AT_ARMORY, AT_LOADOUT, AT_STATION, AT_CUSTOMIZE, AT_TERMINAL, AT_BARS };
static const AtBar kAtBars[AT_BARS] = {
    [AT_ARMORY] = {HF_MENU_ARMORY, 0x60u, 0, 0x570u}, [AT_LOADOUT] = {11u, 0xB0u, 0, 0xF950u},
    [AT_STATION] = {32u, 0x150u, 0, 0x10F8u}, [AT_CUSTOMIZE] = {HF_MENU_CUSTOMIZE, 0x158u, 0x660u, 0x4D0u},
    [AT_TERMINAL] = {34u, 0x68u, 0, 0x4D8u}};

/* Each label list a bar shows: its ids in game.dll as the code that sets it names them, and the HF_MENU_TABS_ value a
 * mod adds to. A value with two lists (the Loadout's) lists the longer first: the shorter is its start. */
typedef struct AtList { uint32_t bar, ids, count, tabs; } AtList;
static const AtList kAtLists[] = {
    {AT_ARMORY, 0x32E2688u, 5, HF_MENU_TABS_ARMORY_CATEGORIES}, {AT_ARMORY, 0x33108B0u, 3, HF_MENU_TABS_ARMORY_WEAPONS},
    {AT_ARMORY, 0x33111F0u, 3, HF_MENU_TABS_ARMORY_ARMOUR}, {AT_ARMORY, 0x32D4E28u, 6, HF_MENU_TABS_ARMORY_COSMETICS},
    {AT_LOADOUT, 0x32E26E8u, 2, HF_MENU_TABS_LOADOUT}, {AT_LOADOUT, 0x32E26E8u, 1, HF_MENU_TABS_LOADOUT},
    {AT_STATION, 0x21E9D00u, 3, HF_MENU_TABS_SPACE_STATION}, {AT_CUSTOMIZE, 0x21E6738u, 2, HF_MENU_TABS_CUSTOMIZE},
    {AT_TERMINAL, 0x328C9ECu, 1, HF_MENU_TABS_INFO_TERMINAL},
    /* SDK 45: the SKINS page, HellForge's own: no game labels; AtShown knows it by its first mod label. */
    {AT_ARMORY, 0, 0, HF_MENU_TABS_ARMORY_SKINS}};
#define AT_LISTS (sizeof kAtLists / sizeof kAtLists[0])
#define AT_KINDS (HF_MENU_TABS_ARMORY_SKINS + 1u)
#define AT_SKINS_LIST (AT_LISTS - 1u)

static const ActPin kAtPins[] = {
    /* The Armory: its select, page open and input, the lists they set and the bar's fields. */
    {0, AT_ARMORY_SELECT, "48895c2418574883ec2080793400488b"}, {0, AT_ARMORY_OPEN, "4056574883ec280fb6f2488bf9385134"},
    {0, AT_ARMORY_INPUT, "4c894424185355574883ec30488bf949"}, {0, 0x1457A61u, "488db770050000488bcee8"},
    {0, 0x1457A75u, "8b97dce50000488bcfe8"}, {0, 0x14573AFu, "488d9f70050000"},
    {0, 0x145740Du, "41b805000000488d156eb2e801"}, {0, 0x145764Du, "41b803000000488d155692eb01"},
    {0, 0x145747Eu, "41b803000000488d15659deb01"}, {0, 0x14573BEu, "41b806000000488d155ddae701"},
    {0, AT_TITLE, "40534883ec30488d9910010000488bcb"}, {0, 0x17AAC85u, "4489a968e00000"}, {0, 0x17AAC99u, "4881c1e8df0000"}, {0, 0x17ABFD4u, "4863816ce00000"},
    /* The Loadout (menu +0xB0): its bar's update reads the index into its select; its labels. */
    {0, 0x14ACE0Cu, "488b89b0000000"}, {0, 0x146D503u, "488d8f50f90000"}, {0, 0x146D519u, "8b9fbcd90100"},
    {0, 0x146D53Eu, "8bd3488bcfe848380000"}, {0, AT_LOADOUT_SELECT, "4c8bdc55535641544157498d6ba84881"},
    {0, 0x1465B9Eu, "488d1543cbe70141b801000000"},
    /* The Democracy Space Station (menu +0x150): index 0..2 picks a page, any other leaves it; its labels. */
    {0, 0x14AD6C4u, "488b8950010000"}, {0, 0x149263Bu, "488d8bf8100000"}, {0, 0x1492652u, "8b8b64f10000"},
    {0, 0x19E987Eu, "41b803000000488d1575048000"},
    /* The weapon customization (menu +0x158, its tabs part +0x660): index 0 or 1 acts, any other leaves it. */
    {0, 0x14AD6D5u, "488bb158010000488d8e60060000"}, {0, 0x19FE195u, "488d8fd0040000"}, {0, 0x19FE1ACu, "8b8f3ce50000"},
    {0, 0x19FD7C3u, "41b802000000488d15688f7e00"},
    /* The Info Terminal (menu +0x68): a tab change does nothing there; its one label. */
    {0, 0x14AD76Bu, "488b71684c8bc7488d8ed8040000"}, {0, 0x1494B4Bu, "41b801000000488d15947edf01"}};

typedef struct AtTab { HfMod* mod; HfArmoryTabCallback callback; void* user; uint32_t kind, text, handle; } AtTab;
typedef void (*AtSelectFn)(uintptr_t screen, uint32_t index);
typedef void (*AtLoadoutSelectFn)(uintptr_t screen, uint32_t index, uint8_t sound);
typedef void (*AtOpenFn)(uintptr_t screen, uint8_t open);
typedef uint64_t (*AtInputFn)(uintptr_t screen, void* input, void* a, void* b);
typedef void (*AtSetFn)(uintptr_t bar, const uint32_t* ids, uint32_t count);
typedef void (*AtPickFn)(uintptr_t bar, uint32_t index, uint8_t a, uint8_t b);
typedef void (*AtTitleFn)(uintptr_t bar, uint32_t text);

static AtTab g_atTabs[AT_TABS];
static uint32_t g_atCount;
static SRWLOCK g_atLock = SRWLOCK_INIT;
static HfMod g_atMod; /* the loader's own tick: installs the hooks and follows the bar on show */
static volatile LONG g_atTicking;
static int g_atState; /* 0 not installed, 1 hooked, -1 off (modloader.log says why) */
static uintptr_t g_atGame;
static uint32_t g_atActive; /* the handle of the mod tab a bar has selected, 0 none */
static AtSelectFn g_atSelect;
static AtLoadoutSelectFn g_atLoadoutSelect;
static AtOpenFn g_atOpen;
static AtInputFn g_atInput;
/* SDK 45, the SKINS page: HellForge's SKINS label on the categories; picked, the weapons page opens (its grid and
 * records, which the mods' tabs rebuild) with the SKINS tabs alone on the bar, until the page closes. */
#define AT_PAGE_OPEN 0x34u  /* in the screen: 1 a page shows, 0 the overview (0x1457350 keeps it) */
#define AT_CATEGORY 0x30u   /* in the screen: the category the overview shows and the page opens (0 weapons) */
static uint32_t g_atSkinsTab;     /* the handle of HellForge's SKINS label on the categories, 0 none */
static uint32_t g_atSkinsText;    /* its text id, also the page's title */
static int g_atSkins;             /* 0 not shown, 1 SKINS picked on the overview (the page opens next), 2 the page */
static uint32_t g_atSkinsIndex;   /* the SKINS page's tab picked last, kept while the Armory is up */
static volatile LONG g_atCalls;   /* inside a mod's callback: a select it makes reaches the game */

/* The vanilla labels a kind's bar shows at most: its mods get the rest of the 8. */
static uint32_t AtVanilla(uint32_t kind)
{
    uint32_t most = 0;
    for (uint32_t i = 0; i < AT_LISTS; i++)
        if (kAtLists[i].tabs == kind && kAtLists[i].count > most) most = kAtLists[i].count;
    return most;
}

static uint32_t AtTabsOf(int list, AtTab out[AT_LABELS]);

/* The list a bar shows (its vanilla labels first, as the game set them), and how many labels it has; -1 none. */
static int AtShown(int bar, uintptr_t at, uint32_t* count, uint32_t ids[AT_LABELS])
{
    if (SafeRead(at + AT_BAR_COUNT, count, 4) || *count == 0 || *count > AT_LABELS || SafeRead(at + AT_BAR_IDS, ids, 4u * *count)) return -1;
    if (bar == AT_ARMORY && g_atSkins == 2)
    {
        AtTab first[AT_LABELS];
        if (AtTabsOf((int)AT_SKINS_LIST, first) && first[0].text == ids[0]) return (int)AT_SKINS_LIST;
    }
    for (int list = 0; list < (int)AT_LISTS; list++)
    {
        uint32_t vanilla[AT_LABELS];
        if (kAtLists[list].count && (int)kAtLists[list].bar == bar && kAtLists[list].count <= *count &&
            !SafeRead(g_atGame + kAtLists[list].ids, vanilla, 4u * kAtLists[list].count) &&
            memcmp(vanilla, ids, 4u * kAtLists[list].count) == 0) return list;
    }
    return -1;
}

/* The mod tabs of a list, in the order they were added: how many fit on the bar after its vanilla labels. */
static uint32_t AtTabsOf(int list, AtTab out[AT_LABELS])
{
    if (list < 0) return 0;
    uint32_t n = 0, room = AT_LABELS - kAtLists[list].count;
    AcquireSRWLockShared(&g_atLock);
    for (uint32_t i = 0; i < g_atCount && n < room; i++)
        if (g_atTabs[i].kind == kAtLists[list].tabs) out[n++] = g_atTabs[i];
    ReleaseSRWLockShared(&g_atLock);
    return n;
}

/* The mods' labels put after the vanilla ones whenever the game set the vanilla list alone, keeping the selection. */
static void AtLabels(int bar, uintptr_t at)
{
    uint32_t count, ids[AT_LABELS], index;
    AtTab tabs[AT_LABELS];
    int list = AtShown(bar, at, &count, ids);
    if (list < 0 || count != kAtLists[list].count) return;
    uint32_t n = AtTabsOf(list, tabs);
    if (n == 0 || SafeRead(at + AT_BAR_INDEX, &index, 4)) return;
    for (uint32_t i = 0; i < n; i++) ids[count + i] = tabs[i].text;
    ((AtSetFn)(g_atGame + AT_SET))(at, ids, count + n);
    ((AtPickFn)(g_atGame + AT_PICK))(at, index < count + n ? index : 0, 1, 1);
}

/* 1 when a select's index is one of the game's labels on the bar shown (or the bar is not one of ours to judge). */
static int AtGameIndex(int bar, uintptr_t at, uint32_t index)
{
    uint32_t count, ids[AT_LABELS];
    int list = AtShown(bar, at, &count, ids);
    return list < 0 || index < kAtLists[list].count || g_atCalls > 0;
}

/* The callbacks when a bar's selection moved onto or off a mod's tab (at 0: no bar of ours is shown). */
static void AtFollow(int bar, uintptr_t at)
{
    uint32_t count, ids[AT_LABELS], index = 0, want = 0;
    AtTab tabs[AT_LABELS], was = {0}, now = {0};
    int list = at ? AtShown(bar, at, &count, ids) : -1;
    if (list >= 0 && !SafeRead(at + AT_BAR_INDEX, &index, 4) && index >= kAtLists[list].count && index < count)
    {
        uint32_t n = AtTabsOf(list, tabs), k = index - kAtLists[list].count;
        if (k < n && tabs[k].text == ids[index]) { now = tabs[k]; want = now.handle; }
    }
    if (want == g_atActive) return;
    AcquireSRWLockShared(&g_atLock);
    for (uint32_t i = 0; i < g_atCount; i++) if (g_atTabs[i].handle == g_atActive) was = g_atTabs[i];
    ReleaseSRWLockShared(&g_atLock);
    g_atActive = want;
    InterlockedIncrement(&g_atCalls);
    if (was.callback) was.callback(was.mod, was.handle, 0, was.user);
    if (now.callback) now.callback(now.mod, now.handle, 1, now.user);
    InterlockedDecrement(&g_atCalls);
}

/* The SKINS page's labels in place of the weapons page's, whenever the game set those (the page opening, its input
 * after the grid, the way back from CUSTOMIZE), keeping the SKINS tab picked last; the page closed ends it. */
static void AtSkinsLabels(uintptr_t screen)
{
    uintptr_t at = screen + kAtBars[AT_ARMORY].bar;
    uint8_t page = 0;
    uint32_t count, ids[AT_LABELS], index = 0;
    AtTab tabs[AT_LABELS];
    if (g_atSkins != 2) return;
    if (SafeRead(screen + AT_PAGE_OPEN, &page, 1) || !page) { g_atSkins = 0; return; }
    int list = AtShown(AT_ARMORY, at, &count, ids);
    uint32_t n = AtTabsOf((int)AT_SKINS_LIST, tabs);
    if (list == (int)AT_SKINS_LIST)
    {
        if (!SafeRead(at + AT_BAR_INDEX, &index, 4) && index < count) g_atSkinsIndex = index;
        return;
    }
    if (list < 0 || kAtLists[list].tabs != HF_MENU_TABS_ARMORY_WEAPONS || n == 0) return; /* another list: left as set */
    for (uint32_t i = 0; i < n; i++) ids[i] = tabs[i].text;
    ((AtSetFn)(g_atGame + AT_SET))(at, ids, n);
    ((AtPickFn)(g_atGame + AT_PICK))(at, g_atSkinsIndex < n ? g_atSkinsIndex : 0, 1, 1);
    ((AtTitleFn)(g_atGame + AT_TITLE))(at, g_atSkinsText); /* the weapons page's open titled it WEAPONRY */
}

/* SKINS picked on the overview: the weapons category is chosen as the game would (its page's records and grid), then
 * the page opens, and the open turns its bar into the SKINS tabs. Run after the game's input or on the tick, never
 * from inside a select. */
static void AtSkinsOpen(uintptr_t screen)
{
    uint8_t page = 1;
    if (g_atSkins != 1 || SafeRead(screen + AT_PAGE_OPEN, &page, 1)) return;
    if (page) { g_atSkins = 0; return; }
    g_atSelect(screen, 0); /* the overview's select (the page is closed): category 0, WEAPONRY */
    uint32_t category = 1;
    if (SafeRead(screen + AT_CATEGORY, &category, 4) || category != 0) { g_atSkins = 0; Note("menu tabs: SKINS: the weapons category did not take"); return; }
    ((AtOpenFn)(g_atGame + AT_ARMORY_OPEN))(screen, 1);
}

/* HellForge's SKINS label on the categories: picked, the page opens next; left before it opened, nothing happens. */
static void AtSkinsPicked(HfMod* mod, uint32_t tab, int selected, void* user)
{
    (void)mod, (void)tab, (void)user;
    if (selected && g_atSkins == 0) g_atSkins = 1;
    else if (!selected && g_atSkins == 1) g_atSkins = 0;
}

/* A click or Q/E onto a mod's label: the Armory's select would read a tab record past its own, so it is not called. */
static void AtSelectDetour(uintptr_t screen, uint32_t index)
{
    uintptr_t at = screen + kAtBars[AT_ARMORY].bar;
    if (AtGameIndex(AT_ARMORY, at, index)) g_atSelect(screen, index);
    AtFollow(AT_ARMORY, at);
}

/* The same on the Loadout: its select would take a mod's index as one of its own pages. */
static void AtLoadoutSelectDetour(uintptr_t screen, uint32_t index, uint8_t sound)
{
    uintptr_t at = screen + kAtBars[AT_LOADOUT].bar;
    if (AtGameIndex(AT_LOADOUT, at, index)) g_atLoadoutSelect(screen, index, sound);
    AtFollow(AT_LOADOUT, at);
}

static void AtOpenDetour(uintptr_t screen, uint8_t open)
{
    g_atOpen(screen, open);
    uint32_t category = 1;
    if (!open) g_atSkins = 0; /* back to the overview: WEAPONRY is picked there, as the game picks it */
    else if (g_atSkins == 1 && !SafeRead(screen + AT_CATEGORY, &category, 4) && category == 0) g_atSkins = 2;
    AtSkinsLabels(screen);
    AtLabels(AT_ARMORY, screen + kAtBars[AT_ARMORY].bar);
    AtFollow(AT_ARMORY, screen + kAtBars[AT_ARMORY].bar);
}

static uint64_t AtInputDetour(uintptr_t screen, void* input, void* a, void* b)
{
    uint64_t result = g_atInput(screen, input, a, b);
    AtSkinsLabels(screen);
    AtLabels(AT_ARMORY, screen + kAtBars[AT_ARMORY].bar);
    AtFollow(AT_ARMORY, screen + kAtBars[AT_ARMORY].bar);
    AtSkinsOpen(screen);
    return result;
}

/* The bar of the screen the menu shows, if it is one of ours: its kind (AT_...) and address, else -1. */
static int AtOnShow(uintptr_t* at)
{
    uintptr_t object, screen;
    *at = 0;
    if (!EvpPtr(g_atGame + WC_MENU, &object)) return -1;
    uint32_t state = EvpU32(object, 0);
    for (int bar = 0; bar < AT_BARS; bar++)
    {
        if (kAtBars[bar].menu != state) continue;
        if (!EvpPtr(object + kAtBars[bar].screen, &screen) || screen == 0) return -1;
        *at = screen + kAtBars[bar].sub + kAtBars[bar].bar;
        return bar;
    }
    return -1;
}

static void AtInstall(void)
{
    g_atGame = ApiModuleBase("game.dll", NULL);
    const char* why = NULL;
    if (!g_atGame || strcmp(g_build, "25480438")) why = "unsupported game build";
    else if (!ActPins(g_atGame, kWcArmoryPins, 1) || !ActPins(g_atGame, kAtPins, sizeof kAtPins / sizeof kAtPins[0])) why = "the menus' code is not the reviewed one";
    else if (LoaderHook("menu tabs: armory select", g_atGame + AT_ARMORY_SELECT, (void*)AtSelectDetour, (void**)&g_atSelect) ||
             LoaderHook("menu tabs: armory page", g_atGame + AT_ARMORY_OPEN, (void*)AtOpenDetour, (void**)&g_atOpen) ||
             LoaderHook("menu tabs: armory input", g_atGame + AT_ARMORY_INPUT, (void*)AtInputDetour, (void**)&g_atInput) ||
             LoaderHook("menu tabs: loadout select", g_atGame + AT_LOADOUT_SELECT, (void*)AtLoadoutSelectDetour, (void**)&g_atLoadoutSelect))
        why = "a hook could not be placed";
    g_atState = why ? -1 : 1;
    if (why) Note("menu tabs: off: %s; mods' tabs are not shown", why);
    else Note("menu tabs: on, %u mod tabs", g_atCount);
}

static void AtTick(HfMod* mod, float dt, void* user)
{
    (void)mod, (void)dt, (void)user;
    if (g_atState == 0 && g_fired[HF_EVENT_GAME_LOADED]) AtInstall();
    if (g_atState <= 0) return;
    uintptr_t at;
    int bar = AtOnShow(&at);
    if (bar != AT_ARMORY && bar != AT_CUSTOMIZE) g_atSkins = 0; /* the Armory left (CUSTOMIZE comes back to the page) */
    if (bar == AT_ARMORY) AtSkinsLabels(at - kAtBars[AT_ARMORY].bar);
    if (bar >= 0) AtLabels(bar, at); /* the list the screen set when it opened, or set again since */
    AtFollow(bar, at);
    if (bar == AT_ARMORY) AtSkinsOpen(at - kAtBars[AT_ARMORY].bar);
}

static void AtModInit(void)
{
    if (g_atMod.loaded) return;
    g_atMod.api = &g_api;
    snprintf(g_atMod.id, sizeof g_atMod.id, "hellforge.menu-tabs");
    g_atMod.log = g_log;
    g_atMod.loaded = 1;
}

/* SDK 45: HellForge's own SKINS label on the categories, added with the first SKINS tab; 0 when the bar is full. */
static uint32_t AtSkinsCategory(void)
{
    if (g_atSkinsTab) return g_atSkinsTab;
    AtModInit();
    uint32_t text = ApiAddText(&g_atMod, "armory_tab/skins", "SKINS"), taken = 0, room = AT_LABELS - AtVanilla(HF_MENU_TABS_ARMORY_CATEGORIES);
    if (text == 0) return 0;
    AcquireSRWLockExclusive(&g_atLock);
    for (uint32_t i = 0; i < g_atCount; i++) taken += g_atTabs[i].kind == HF_MENU_TABS_ARMORY_CATEGORIES;
    if (g_atSkinsTab == 0 && g_atCount < AT_TABS && taken < room)
    {
        g_atTabs[g_atCount] = (AtTab){&g_atMod, AtSkinsPicked, NULL, HF_MENU_TABS_ARMORY_CATEGORIES, text, g_atCount + 1};
        g_atSkinsTab = ++g_atCount;
        g_atSkinsText = text;
    }
    ReleaseSRWLockExclusive(&g_atLock);
    if (g_atSkinsTab) Note("menu tabs: SKINS on the Armory's categories (tab %u)", g_atSkinsTab);
    return g_atSkinsTab;
}

/* SDK 27, add_menu_tab (and SDK 26 add_armory_tab, its first four kinds): a label of the mod's on one of the game's
 * tab bars; its handle, or 0 refused (logged). */
static uint32_t ApiAddMenuTab(HfMod* mod, uint32_t kind, const char* label, HfArmoryTabCallback on_select, void* user)
{
    const int armory = kind <= HF_MENU_TABS_ARMORY_COSMETICS || kind == HF_MENU_TABS_ARMORY_SKINS;
    const char* call = armory ? "add_armory_tab" : "add_menu_tab";
    if (mod == NULL || kind >= AT_KINDS || label == NULL || label[0] == 0 || on_select == NULL)
    { ApiLog(mod, "add_menu_tab: refused: it needs an HF_MENU_TABS_ bar, a label and a callback"); return 0; }
    if (g_atState < 0) { ApiLog(mod, "%s(%s): refused: menu tabs are off (modloader.log says why)", call, label); return 0; }
    char key[160];
    /* The Armory's keep SDK 26's text keys, so a label's id stays the same. */
    snprintf(key, sizeof key, "%s/%u/%s", armory ? "armory_tab" : "menu_tab", kind, label);
    if (kind == HF_MENU_TABS_ARMORY_SKINS && AtSkinsCategory() == 0)
    { ApiLog(mod, "%s(%s): refused: the categories bar has no room for SKINS", call, label); return 0; }
    uint32_t text = ApiAddText(mod, key, label), handle = 0, taken = 0, room = AT_LABELS - AtVanilla(kind);
    if (text == 0) return 0;
    AcquireSRWLockExclusive(&g_atLock);
    for (uint32_t i = 0; i < g_atCount; i++) taken += g_atTabs[i].kind == kind;
    if (g_atCount < AT_TABS && taken < room)
    {
        handle = g_atCount + 1;
        g_atTabs[g_atCount++] = (AtTab){mod, on_select, user, kind, text, handle};
    }
    ReleaseSRWLockExclusive(&g_atLock);
    if (handle == 0) { ApiLog(mod, "%s(%s): refused: that bar holds %u labels, and mods took the rest", call, label, AT_LABELS); return 0; }
    if (InterlockedExchange(&g_atTicking, 1) == 0)
    {
        AtModInit();
        ApiOnGameThread(&g_atMod, AtTick, NULL);
    }
    ApiLog(mod, "%s(%s): tab %u on bar %u", call, label, handle, kind);
    return handle;
}

/* SDK 26, add_armory_tab: the Armory's four lists only. */
static uint32_t ApiAddArmoryTab(HfMod* mod, uint32_t list, const char* label, HfArmoryTabCallback on_select, void* user)
{
    if (list > HF_MENU_TABS_ARMORY_COSMETICS && list != HF_MENU_TABS_ARMORY_SKINS)
    { ApiLog(mod, "add_armory_tab: refused: it needs an HF_ARMORY_TABS_ list (add_menu_tab takes the other bars)"); return 0; }
    return ApiAddMenuTab(mod, list, label, on_select, user);
}
