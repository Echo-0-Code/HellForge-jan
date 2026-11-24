/* SDK 41, add_hangar_category: a category of a mod's own in Ship Management > Vehicle Customization, for game.dll
 * build 25480438 (docs/modding/ui.md "A category in Vehicle Customization"). The screen's controller keeps the picked
 * category's index at +0x98 and builds the cards (0x14DC620) from the first unit skin collection whose type is
 * kHgTypes[index]; index 5, a 6th tab the mod's xaml edit adds, reads the table's padding: type 0, the Pelican's. So
 * while the controller is on index 5, the builder and the thumbnail update run with HellForge's record in the
 * Pelican's place for that call only, the card state asks our pick, and EQUIP keeps the pick ours: the game's pick
 * array and its sync stay the Pelican's. The 12-collection array is never grown (0x6EE790 reads exactly 12).
 * Thumbnails and the 3D preview are left out on our cards. Included by world_calls.c, after menu_tabs.c. */

#define HG_COLLECTIONS 0x37C8180u   /* 12 unit skin collection records: +4 group, +8 type, +0x18 name key, +0x28 skins */
#define HG_COLLECTION_COUNT 0x348E3F8u
#define HG_SLOTS 12u
#define HG_INDEX 0x98u              /* the controller's category index */
#define HG_EQUIPPED_AT 0x60u        /* the controller's equipped card of index 5 (+0x38 + 8 * index) */
#define HG_CARD_SELECTED 0x30u      /* the controller's card under the cursor */
#define HG_CARD_STATE 0x48u         /* a card's state: 0 equipped, 1 owned, 2 low funds, 3/4 locked, 5 acquire */
#define HG_CARD_ID 0xB8u
#define HG_OURS 5u
#define HG_CARDS 24u
#define HG_ROW 0x40u                /* a skin row: +8 id, +0x20 name key */
#define HG_RECORD 0x40u

#define HG_BUILD 0x14DC620u         /* (controller, view) */
#define HG_THUMBS 0x14DD120u        /* (controller, render slot) */
#define HG_STATE 0x14DC000u         /* (record, skin) -> card state */
#define HG_EQUIP 0x14DD660u         /* (controller): EQUIP on the card under the cursor */
#define HG_QUEUE 0x14CF360u         /* a row's thumbnail render request */
#define HG_PREVIEW 0x14C9490u       /* the 3D preview of the equipped card */
#define HG_SET_STATE 0x155A470u     /* (card, state) */

static const ActPin kHgPins[] = {
    {0, HG_BUILD, "48895c24185556574154415541564157"}, {0, HG_THUMBS, "8954241048894c240853555657415441"},
    {0, HG_STATE, "40534883ec204c8b1d430efa014533c9"}, {0, HG_EQUIP, "40574883ec20488b5130488bf94885d2"},
    {0, HG_QUEUE, "4883ec7833c0488d5114660f1f440000"}, {0, HG_SET_STATE, "40534883ec20488bd9395148744a8951"},
    {0, HG_PREVIEW, "48895c240848896c2410488974241857"},
    /* the builder and the thumbnail update read the category tables here; index 5 reads their padding, a 0 */
    {0, 0x22F3908u, "000000000100000003000000040000000700000000000000"},
    {0, 0x22EA400u, "000000000100000003000000040000000700000000000000"}};

typedef void (*HgBuildFn)(uint8_t* controller, uint8_t* view);
typedef void (*HgThumbsFn)(uint8_t* controller, uint32_t slot);
typedef uint64_t (*HgStateFn)(const uint8_t* record, uint32_t skin);
typedef void (*HgEquipFn)(uint8_t* controller);
typedef void (*HgQueueFn)(uint8_t* row, uint64_t a, uint64_t key, uint32_t b, uint32_t c, uint32_t d, uint32_t e);
typedef void (*HgPreviewFn)(uint8_t* at, uint32_t type, const uint8_t* row);
typedef void (*HgSetStateFn)(uint8_t* card, uint32_t state);

typedef struct HgCategory
{
    HfMod* mod;
    HfHangarPick onPick;
    void* user;
    uint32_t count, equipped;
    uint32_t cards[HG_CARDS];
    char names[HG_CARDS][64];
    char name[64];
    uint8_t rows[HG_CARDS * HG_ROW];
    uint8_t record[HG_RECORD];
    int built;
} HgCategory;

static HgCategory g_hgCategory;
static int g_hgAdded;
static HfMod g_hgMod;
static int g_hgState; /* 0 not installed, 1 hooked, -1 off (modloader.log says why) */
static int g_hgInOurs; /* the builder is running for our category: no thumbnails or preview */
static HgBuildFn g_hgBuild;
static HgThumbsFn g_hgThumbs;
static HgStateFn g_hgState0;
static HgEquipFn g_hgEquip;
static HgQueueFn g_hgQueue;
static HgPreviewFn g_hgPreview;
static HgSetStateFn g_hgSetState;

/* The game's collection slot of type 0 (the Pelican's), or -1. */
static int HgPelicanSlot(void)
{
    uint32_t count = 0;
    if (SafeRead(g_game + HG_COLLECTION_COUNT, &count, 4)) return -1;
    for (uint32_t k = 0; k < count && k < HG_SLOTS; k++)
    {
        uint8_t* record = NULL;
        uint32_t type = 1;
        if (SafeRead(g_game + HG_COLLECTIONS + 8u * k, &record, 8) || record == NULL || SafeRead((uintptr_t)record + 8, &type, 4)) continue;
        if (type == 0) return (int)k;
    }
    return -1;
}

/* Our record and rows, from the Pelican's: its record's first bytes and its first skin row, with our ids and names. */
static int HgMake(int slot)
{
    HgCategory* c = &g_hgCategory;
    const uint8_t* pelican = *(uint8_t* const*)(g_game + HG_COLLECTIONS + 8u * (uint32_t)slot);
    const uint8_t* skins = NULL;
    if (SafeRead((uintptr_t)pelican + 0x28, &skins, 8) || skins == NULL || SafeRead((uintptr_t)pelican, c->record, HG_RECORD)) return 0;
    for (uint32_t i = 0; i < c->count; i++)
    {
        uint8_t* row = c->rows + (size_t)i * HG_ROW;
        if (SafeRead((uintptr_t)skins, row, HG_ROW)) return 0;
        memcpy(row + 8, &c->cards[i], 4);
        const char* key = c->names[i];
        memcpy(row + 0x20, &key, 8);
    }
    const char* name = c->name;
    const uint8_t* rows = c->rows;
    memcpy(c->record + 0x18, &name, 8);
    memcpy(c->record + 0x28, &rows, 8);
    memcpy(c->record + 0x30, &c->count, 4);
    return c->built = 1;
}

static int HgOurs(const uint8_t* controller)
{
    uint32_t index = 0;
    return g_hgAdded && controller != NULL && !SafeRead((uintptr_t)controller + HG_INDEX, &index, 4) && index == HG_OURS;
}

/* Runs one of the screen's calls with our record in the Pelican's slot. */
static int HgSwapIn(uint8_t** saved)
{
    int slot = HgPelicanSlot();
    if (slot < 0 || (!g_hgCategory.built && !HgMake(slot))) return -1;
    uint8_t** at = (uint8_t**)(g_game + HG_COLLECTIONS + 8u * (uint32_t)slot);
    *saved = *at;
    *at = g_hgCategory.record;
    return slot;
}

static void HgSwapOut(int slot, uint8_t* saved) { *(uint8_t**)(g_game + HG_COLLECTIONS + 8u * (uint32_t)slot) = saved; }

static void HgBuildDetour(uint8_t* controller, uint8_t* view)
{
    uint8_t* saved = NULL;
    int slot = HgOurs(controller) ? HgSwapIn(&saved) : -1;
    if (slot < 0) { g_hgBuild(controller, view); return; }
    g_hgInOurs = 1;
    g_hgBuild(controller, view);
    g_hgInOurs = 0;
    HgSwapOut(slot, saved);
}

static void HgThumbsDetour(uint8_t* controller, uint32_t render)
{
    uint8_t* saved = NULL;
    int slot = HgOurs(controller) ? HgSwapIn(&saved) : -1;
    g_hgThumbs(controller, render);
    if (slot >= 0) HgSwapOut(slot, saved);
}

static uint64_t HgStateDetour(const uint8_t* record, uint32_t skin)
{
    if (record != g_hgCategory.record) return g_hgState0(record, skin);
    return skin == g_hgCategory.equipped ? 0 : 1;
}

static void HgQueueDetour(uint8_t* row, uint64_t a, uint64_t key, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    if (!g_hgInOurs) g_hgQueue(row, a, key, b, c, d, e);
}

static void HgPreviewDetour(uint8_t* at, uint32_t type, const uint8_t* row)
{
    if (!g_hgInOurs) g_hgPreview(at, type, row);
}

/* EQUIP in our category: the card becomes the equipped one and the mod hears of it; the game's picks are untouched. */
static void HgEquipDetour(uint8_t* controller)
{
    if (!HgOurs(controller)) { g_hgEquip(controller); return; }
    uint8_t* card = *(uint8_t**)(controller + HG_CARD_SELECTED);
    if (card == NULL || *(const uint32_t*)(card + HG_CARD_STATE) != 1) return;
    HgCategory* c = &g_hgCategory;
    c->equipped = *(const uint32_t*)(card + HG_CARD_ID);
    g_hgSetState(card, 0);
    uint8_t* before = *(uint8_t**)(controller + HG_EQUIPPED_AT);
    if (before != NULL && before != card) g_hgSetState(before, 1);
    *(uint8_t**)(controller + HG_EQUIPPED_AT) = card;
    if (c->onPick) c->onPick(c->mod, 1, c->equipped, c->user);
}

static void HgInstall(void)
{
    const char* why = NULL;
    if (!g_game || strcmp(g_build, "25480438")) why = "unsupported game build";
    else if (!ActPins(g_game, kHgPins, sizeof kHgPins / sizeof kHgPins[0])) why = "the screen's code is not the reviewed one";
    else
    {
        g_hgSetState = (HgSetStateFn)(g_game + HG_SET_STATE);
        if (LoaderHook("hangar: cards", g_game + HG_BUILD, (void*)HgBuildDetour, (void**)&g_hgBuild) ||
            LoaderHook("hangar: thumbnails", g_game + HG_THUMBS, (void*)HgThumbsDetour, (void**)&g_hgThumbs) ||
            LoaderHook("hangar: card state", g_game + HG_STATE, (void*)HgStateDetour, (void**)&g_hgState0) ||
            LoaderHook("hangar: equip", g_game + HG_EQUIP, (void*)HgEquipDetour, (void**)&g_hgEquip) ||
            LoaderHook("hangar: thumbnail request", g_game + HG_QUEUE, (void*)HgQueueDetour, (void**)&g_hgQueue) ||
            LoaderHook("hangar: preview", g_game + HG_PREVIEW, (void*)HgPreviewDetour, (void**)&g_hgPreview))
            why = "a hook could not be placed";
    }
    g_hgState = why ? -1 : 1;
    if (why) Note("hangar: off: %s; the mod's Vehicle Customization category is not shown", why);
    else Note("hangar: on, category \"%s\" with %u cards", g_hgCategory.name, g_hgCategory.count);
}

static void HgTick(HfMod* mod, float dt, void* user)
{
    (void)mod, (void)dt, (void)user;
    if (g_hgState == 0 && g_fired[HF_EVENT_GAME_LOADED]) HgInstall();
}

/* SDK 41, add_hangar_category: the handle (1), or 0 refused (logged). */
static uint32_t ApiAddHangarCategory(HfMod* mod, const HfHangarCategory* def)
{
    if (mod == NULL || def == NULL || def->size < sizeof(HfHangarCategory) || def->name == NULL || def->cards == NULL || def->names == NULL ||
        def->count == 0 || def->count > HG_CARDS)
    { ApiLog(mod, "add_hangar_category: refused: it needs its size, a name and 1 to %u cards with names", HG_CARDS); return 0; }
    if (g_fired[HF_EVENT_ENTITIES_READY]) { ApiLog(mod, "add_hangar_category: too late; call it from hellforge_mod_init"); return 0; }
    if (InterlockedExchange((volatile LONG*)&g_hgAdded, 1)) { ApiLog(mod, "add_hangar_category: refused: another mod has the one category"); return 0; }
    HgCategory* c = &g_hgCategory;
    c->mod = mod, c->onPick = def->on_pick, c->user = def->user, c->count = def->count, c->equipped = def->equipped;
    snprintf(c->name, sizeof c->name, "%s", def->name);
    for (uint32_t i = 0; i < def->count; i++)
    {
        c->cards[i] = def->cards[i];
        snprintf(c->names[i], sizeof c->names[i], "%s", def->names[i] ? def->names[i] : "");
    }
    g_hgMod.api = &g_api;
    snprintf(g_hgMod.id, sizeof g_hgMod.id, "hellforge.hangar");
    g_hgMod.log = g_log;
    g_hgMod.loaded = 1;
    ApiOnGameThread(&g_hgMod, HgTick, NULL);
    ApiLog(mod, "add_hangar_category(%s): %u cards, in Vehicle Customization's 6th tab", c->name, c->count);
    return 1;
}

/* SDK 41, set_hangar_equipped: the card shown as equipped from the next build of the cards. */
static int ApiSetHangarEquipped(HfMod* mod, uint32_t category, uint32_t card)
{
    if (category != 1 || !g_hgAdded || g_hgCategory.mod != mod) { ApiLog(mod, "set_hangar_equipped: refused: not this mod's category"); return 1; }
    g_hgCategory.equipped = card;
    return 0;
}

#define HG_API ApiAddHangarCategory, ApiSetHangarEquipped
