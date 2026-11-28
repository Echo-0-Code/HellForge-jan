/* What a search box needs of the lists (list_filter.c, menu_tabs.c) without the game: a stratagem row named with its
 * payload's designation as the picker's label finds it, the SKINS page counted as a list that is up, and its picked
 * tab asked again for a rebuild. The game's lookups are stand-ins and every text a mod text. Included by stats_test.c. */

#define LST_TITLE 0x7E570001u
#define LST_MG 0x7E570002u
#define LST_SENTRY 0x7E570003u

static uint8_t g_lstOwn[0x30], g_lstCarried[0x30], g_lstStandIn[0x238];
static uint8_t* LstRecord(uint64_t entity) { return entity == 0xB2 ? g_lstOwn : entity == 0xC3 ? g_lstCarried : NULL; }
static uint8_t* LstStandIn(uint64_t entity) { return entity == 0xA1 ? g_lstStandIn : NULL; }

static int LstNamed(const char* want)
{
    char name[LF_NAME];
    LfStratagemName(1, name, sizeof name);
    return strcmp(name, want) == 0;
}

static void LstStratagemNames(void)
{
    static uint8_t row[0x110];
    static uint64_t payload[3] = {0, 0xA1, 0xB2};
    uint8_t* rows[2] = {NULL, row};
    uint8_t* const savedTable = g_stratagemTable;
    const int savedTexts = g_textCount;
    const uint32_t title = LST_TITLE, mg = LST_MG, sentry = LST_SENTRY, three = 3, one = 1, none = 0;
    const uint64_t carried = 0xC3;
    const uint64_t* list = payload;
    assert(AddTextAs("Machine Gun", LST_TITLE) && AddTextAs("MG-43", LST_MG) && AddTextAs("A/MG-43", LST_SENTRY));
    g_stratagemTable = (uint8_t*)rows;
    memcpy(row + 0x2C, &title, 4);
    memcpy(row + 0x98, &list, 8);
    memcpy(row + 0xA0, &three, 4);
    memcpy(g_lstOwn + 0x24, &mg, 4);
    memcpy(g_lstCarried + 0x24, &sentry, 4);
    memcpy(g_lstStandIn, &carried, 8);

    g_lfRecordOf = NULL, g_lfStandInOf = NULL;
    assert(LstNamed("Machine Gun")); /* the lookups did not match: the title alone, as before */
    g_lfRecordOf = LstRecord;
    assert(LstNamed("MG-43 Machine Gun")); /* payload 0 and 1 have no record: the first that has one names it */
    g_lfStandInOf = LstStandIn;
    assert(LstNamed("MG-43 Machine Gun")); /* a stand-in record that is not set names no one */
    memcpy(g_lstStandIn + 0x22C, &one, 4);
    assert(LstNamed("A/MG-43 Machine Gun")); /* payload 1 stands for an entity with a record: it comes first */
    memcpy(g_lstCarried + 0x24, &none, 4);
    assert(LstNamed("Machine Gun")); /* the first record has no designation: the game shows the title alone too */
    memcpy(g_lstCarried + 0x24, &sentry, 4);
    char lower[LF_NAME];
    LfStratagemName(1, lower, sizeof lower);
    LfLower(lower);
    assert(LfMatches("a/mg-43", lower, "", "") && LfMatches("machine", lower, "", "") && !LfMatches("eat-17", lower, "", ""));

    const uintptr_t bad = 8; /* a payload list that cannot be read, and one with nothing in it */
    memcpy(row + 0x98, &bad, 8);
    assert(LstNamed("Machine Gun"));
    memcpy(row + 0x98, &list, 8);
    memcpy(row + 0xA0, &none, 4);
    assert(LstNamed("Machine Gun"));

    g_lfRecordOf = NULL, g_lfStandInOf = NULL;
    g_stratagemTable = savedTable;
    g_textCount = savedTexts;
}

static int g_lstPicks, g_lstInside;
static void LstTab(HfMod* mod, uint32_t tab, int selected, void* user)
{
    (void)mod, (void)tab, (void)user;
    g_lstPicks += selected ? 1 : 100;
    g_lstInside = (int)g_atCalls;
}

static void LstSkinsPage(void)
{
    static HfMod mod;
    uint8_t* screen = (uint8_t*)calloc(1, LF_GRID_TYPE + 8);
    assert(screen != NULL && g_atCount < AT_TABS);
    static const uint32_t types[4] = {1, 3, 4, 0xFFFFFFFFu};
    for (int i = 0; i < 4; i++) memcpy(screen + LF_TAB_TYPES + (size_t)i * 0x3010u, &types[i], 4);
    uint32_t built = 3;
    screen[0x34] = 1;
    memcpy(screen + LF_GRID_TYPE, &built, 4);
    const int savedSkins = g_atSkins;
    const uint32_t savedCount = g_atCount, savedActive = g_atActive;
    g_atSkins = 0;
    assert(LfArmoryTab((uintptr_t)screen) == 1);
    built = 2; /* the type a SKINS tab's mod builds: no tab of the weapons page has it */
    memcpy(screen + LF_GRID_TYPE, &built, 4);
    assert(LfArmoryTab((uintptr_t)screen) == -1 && !AtSkinsRepick());
    g_atSkins = 2;
    assert(LfArmoryTab((uintptr_t)screen) == LF_TAB_SKINS);
    screen[0x34] = 0; /* the overview: no list */
    assert(LfArmoryTab((uintptr_t)screen) == -1);

    const uint32_t handle = g_atCount + 1;
    g_atTabs[g_atCount++] = (AtTab){&mod, LstTab, NULL, HF_MENU_TABS_ARMORY_SKINS, 1, handle};
    g_lstPicks = 0;
    g_atActive = 0;
    assert(!AtSkinsRepick() && g_lstPicks == 0); /* no tab followed yet: no one to ask */
    g_atActive = handle;
    assert(AtSkinsRepick() && g_lstPicks == 1 && g_lstInside == 1 && g_atCalls == 0); /* selected 1, inside a callback */
    g_atTabs[savedCount].kind = HF_MENU_TABS_ARMORY_WEAPONS; /* only a SKINS tab builds that page's grid */
    assert(!AtSkinsRepick() && g_lstPicks == 1);
    g_atTabs[savedCount].kind = HF_MENU_TABS_ARMORY_SKINS;
    mod.removing = 1;
    assert(!AtSkinsRepick() && g_lstPicks == 1);
    mod.removing = 0;
    g_atSkins = 0;
    assert(!AtSkinsRepick() && g_lstPicks == 1);

    g_atCount = savedCount, g_atActive = savedActive, g_atSkins = savedSkins;
    free(screen);
}

static void ListSearchFixture(void)
{
    LstStratagemNames();
    LstSkinsPage();
}
