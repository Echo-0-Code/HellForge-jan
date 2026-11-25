/* SDK 42, add_loadout_box and set_loadout_layout: boxes of mods' own after THROWABLE on ARMORY > WEAPONRY, for
 * game.dll build 25480438 (docs/internals-world.md "The WEAPONRY overview's boxes"). The overview (screen + 0x44698)
 * holds exactly 3 boxes of 0x3010 and its code counts to 3, so the extra boxes live here and are built, set up, updated
 * and highlighted by the game's own per-box calls. Each hangs off the box before it, as the game chains its own.
 * The game's boxes fill its preview slot (each takes a whole row of the 2 x 3 cell grid), so ours take a slot of
 * their own while the overview shows.
 * Included by world_calls.c after menu_tabs.c (its bar pick and follow). */

#define LB_BOXES HF_LOADOUT_EXTRA
#define LB_BOX 0x3010u
#define LB_CELL 0x50u
#define LB_SCREEN_O 0x44698u      /* the overview inside the Armory screen */
#define LB_O_BOXES 0x330u
#define LB_O_FRAME 0x110u         /* 1520 wide: the hover area */
#define LB_O_ROW 0x220u           /* 1180 wide: the boxes' parent, centred */
#define LB_O_SLOT 0x9450u         /* its preview renderer slot, -1 none */
#define LB_O_HIDDEN 0x9454u
#define LB_O_SELECTED 0x9458u     /* the game's selected box, -1 none */
#define LB_O_MODE 0x945Cu         /* 0 WEAPONRY, 1 APPAREL */
#define LB_BOX_TYPE 0x3000u
#define LB_BOX_ID 0x3004u
#define LB_BOX_TRANSFORM 0x1158u
#define LB_BOX_CELL 0x1918u
#define LB_BOX_ITEM 0x19D0u       /* the item art widget's item (+0x9C0 + 0x1010), shown item, kind */
#define LB_KIND 3u                /* drawn as a secondary weapon's box: the art and its framing */
#define LB_UI_ID 0x58EFE12Eu
#define LB_MOUSE 0x347CF18u       /* +0xE1AD4: 1 while the menus follow the mouse */
#define LB_MENU 0x347CE38u        /* +0x60 the Armory screen, +0x5F0 the cursor */
#define LB_SOUND_MOVE 0xA03A5EDFu
#define LB_RENDERER 0x347CD80u    /* the preview renderer: 6 slots of 0x718 (state +0x728), cell marks +0x2AB0 */
#define LB_SLOTS 6u
#define LB_NONE 0xFFFFFFFFu

#define LB_CONSTRUCT 0x18E18F0u   /* (O, position, anchor, pivot) */
#define LB_SETUP 0x18E1FF0u       /* (O): labels, items and preview cells, on each show */
#define LB_UPDATE 0x1456BB0u      /* (screen, dt) */
#define LB_NAV 0x18E2DE0u         /* (O, input, cursor) -> 1 moved, 2 confirm */
#define LB_SELECT 0x18E2F90u      /* (O, index, sound) */
#define LB_HIDE 0x18E3110u        /* (O, hide) */
#define LB_HIGHLIGHT 0x18E1150u   /* (box, on): animates the box between 326 and 500 wide */
#define LB_ANIMATE 0x1451BB0u     /* (widget, from, to, on, ...): a size animation */

static const ActPin kLbPins[] = {
    {0, LB_CONSTRUCT, "488bc448895810488968184889702057"}, {0, LB_SETUP, "4055535641564157488d6c24c94881ec"},
    {0, LB_UPDATE, "40534883ec60488b056b620202488bd9"}, {0, LB_NAV, "48895c24084889742410574883ec2048"},
    {0, LB_SELECT, "48896c241857415641574883ec208ba9"}, {0, LB_HIDE, "488bc448896818574883ec700fb6ea48"},
    {0, LB_HIGHLIGHT, "4c8bdc555657498d6ba14881ecb00000"},
    {0, 0x18E0900u, "48895c24105556574883ec600f297424"}, {0, 0x14476A0u, "48895c241848896c2420488954241056"},
    {0, 0x144F160u, "4883ec28f30f10412c4c8bc948895424"}, {0, 0x144C5C0u, "40534883ec204c8bca488bd9488b91e0"},
    {0, 0x144C2C0u, "488b81e00000004532c94c8bc14885c0"}, {0, 0x1447160u, "48895c241848896c2420488954241056"},
    {0, 0x1446D10u, "8b014c8bc9c1e809a8010f84a3000000"}, {0, 0x14509E0u, "48895c24088b01c1e809a801756f4c8b"},
    {0, 0x18DE230u, "48895c241048896c2418488974242057"}, {0, 0x18DF2F0u, "48895c240848896c2410488974241857"},
    {0, 0x178E170u, "48895c2408488974241048897c241855"}, {0, 0x18E10B0u, "48895c2408574883ec20488d99581100"},
    {0, 0x18DC5D0u, "40534883ec20488bd9e872700600488b"}, {0, 0x178F970u, "48895c24104889742418574883ec204c"},
    {0, 0x178E020u, "40534883ec20488bd948b90000000004"}, {0, 0x585B80u, "83f90c771d4c8d0574a4a7ff8bc1418b"},
    {0, 0x18E1600u, "48895c241048896c2418565741574883"}, {0, 0x1327F50u, "48895c24084889742410574883ec2048"},
    {0, 0x1448AD0u, "40574883ec20f30f104144488bf90f2e"}, {0, 0x1943710u, "4c8b056996b30133c04c8bd1458b8880"},
    {0, 0x13918E0u, "488bc455574881ecb800000041b8ffff"}, {0, 0x1391890u, "4c8b05e9b40e028bc24869c818070000"},
    /* the screen update's box loop and the constructor's chain step, as traced */
    {0, 0x1456D40u, "0f28ce488bcfe885584800"}, {0, 0x18E1D15u, "48c78424a000000000006041"}};

typedef void (*LbConstructFn)(uintptr_t o, uint64_t position, uint64_t anchor, uint64_t pivot);
typedef void (*LbOFn)(uintptr_t o);
typedef void (*LbUpdateFn)(uintptr_t screen, float dt);
typedef uint64_t (*LbNavFn)(uintptr_t o, uintptr_t input, uintptr_t cursor);
typedef void (*LbSelectFn)(uintptr_t o, uint32_t index, uint8_t sound);
typedef void (*LbHideFn)(uintptr_t o, uint8_t hide);
typedef void (*LbHighlightFn)(uintptr_t box, uint8_t on);
typedef void (*LbAnimateFn)(uintptr_t widget, uint64_t from, uint64_t to, uint8_t on, uint32_t a, uint32_t b, void* c);
typedef void (*LbBoxInitFn)(uintptr_t box, uint64_t unused, uint32_t id);
typedef void (*LbVecFn)(uintptr_t widget, uint64_t xy);
typedef void (*LbLinkFn)(uintptr_t parent, uintptr_t child);
typedef void (*LbRegisterFn)(uintptr_t widget, uint32_t id, uint64_t a, uint64_t b);
typedef void (*LbLabelFn)(uintptr_t label, uint32_t text, uint32_t sub);
typedef int* (*LbArtFn)(uintptr_t art, int* out);
typedef void (*LbCellFn)(uint32_t item, uint32_t slot, const uint8_t* desc, uintptr_t cell, uint8_t flag);
typedef void (*LbBindFn)(uintptr_t box, uintptr_t cell);
typedef void (*LbFrameFn)(uintptr_t transform, float dt);
typedef uint8_t (*LbHitFn)(uintptr_t cursor, uintptr_t widget, uint64_t flag);
typedef uint32_t (*LbDirFn)(uintptr_t input);
typedef uint32_t (*LbActionFn)(uint64_t action);
typedef void (*LbSoundFn)(uint64_t unused, uint32_t event);
typedef void (*LbAlphaFn)(uintptr_t widget, float alpha);
typedef void (*LbFreeFn)(uintptr_t renderer, uint32_t slot);
typedef void (*LbSubmitFn)(uint64_t unused, uint32_t slot);

typedef struct LbBox { HfMod* mod; HfLoadoutBoxItem itemFn; void* user; uint32_t tab, text, handle; } LbBox;

static _Alignas(16) uint8_t g_lbMem[LB_BOXES][LB_BOX];
static _Alignas(16) uint8_t g_lbCells[LB_BOXES][LB_CELL];
static LbBox g_lbBoxes[LB_BOXES];
static uint32_t g_lbCount;
static HfLoadoutLayout g_lbLayout;
static SRWLOCK g_lbLock = SRWLOCK_INIT;
static HfMod g_lbMod;
static int g_lbState;           /* 0 not installed, 1 hooked, -1 off (modloader.log says why) */
static uintptr_t g_lbO;         /* the overview our boxes were built into, 0 none yet */
static int g_lbSel = -1;        /* our selected box, -1 none */
static int g_lbShown;           /* our boxes are set up on a WEAPONRY overview now */
static uint32_t g_lbSlot = LB_NONE; /* our preview renderer slot while they show */
static LbConstructFn g_lbConstruct;
static LbOFn g_lbSetup;
static LbUpdateFn g_lbUpdate;
static LbNavFn g_lbNav;
static LbSelectFn g_lbSelect;
static LbHideFn g_lbHide;
static LbHighlightFn g_lbHighlight;
static LbAnimateFn g_lbAnimate;
static uintptr_t g_lbAnimating; /* the box 0x18E1150 animates now, 0 none */

#define LB_CALL(type, rva) ((type)(g_game + (rva)))

static uintptr_t LbBoxAt(uint32_t k) { return (uintptr_t)g_lbMem[k]; }
static uintptr_t LbGameBox(uintptr_t o, uint32_t i) { return o + LB_O_BOXES + i * LB_BOX; }
static uint64_t LbXY(float x, float y) { uint64_t v; float xy[2] = {x, y}; memcpy(&v, xy, 8); return v; }
static float LbOr(float value, float game) { return value > 0.0f ? value : game; }

/* The box before k in its row, or the one above it when k starts the second row (columns from set_loadout_layout). */
static uintptr_t LbParentOf(uintptr_t o, uint32_t k, int* below)
{
    uint32_t i = 3 + k, columns = g_lbLayout.columns;
    *below = columns >= 3 && i >= columns && i % columns == 0;
    uint32_t parent = *below ? i - columns : i - 1;
    return parent < 3 ? LbGameBox(o, parent) : LbBoxAt(parent - 3);
}

/* After the game built its 3 boxes: ours, built and chained the same way. */
static void LbBuild(uintptr_t o)
{
    float gap = LbOr(g_lbLayout.gap, 14.0f), height = LbOr(g_lbLayout.box_height, 770.0f);
    for (uint32_t k = 0; k < g_lbCount; k++)
    {
        uintptr_t box = LbBoxAt(k);
        int below;
        uintptr_t parent = LbParentOf(o, k, &below);
        memset(g_lbMem[k], 0, LB_BOX);
        LB_CALL(LbBoxInitFn, 0x18E0900u)(box, 0, 3 + k);
        LB_CALL(LbVecFn, 0x14476A0u)(box, below ? LbXY(0.0f, -(height + gap)) : LbXY(gap, 0.0f));
        LB_CALL(LbVecFn, 0x144F160u)(box, below ? LbXY(0.0f, 0.0f) : LbXY(1.0f, 0.0f));
        LB_CALL(LbLinkFn, 0x144C5C0u)(parent, box);
        LB_CALL(LbOFn, 0x144C2C0u)(parent);
    }
    g_lbO = o, g_lbSel = -1, g_lbShown = 0;
    Note("loadout boxes: %u built after the overview's 3", g_lbCount);
}

/* The row's and the hover area's sizes for every box, and the sizes set_loadout_layout gives the boxes; the game's
 * own on the APPAREL overview, which shares these widgets. */
static void LbLayoutSizes(uintptr_t o, int ours)
{
    static const HfLoadoutLayout game = {sizeof(HfLoadoutLayout), 0, 0.0f, 0.0f, 0.0f, 0.0f};
    const HfLoadoutLayout* l = ours ? &g_lbLayout : &game;
    float w = LbOr(l->box_width, 326.0f), wide = LbOr(l->wide_width, 500.0f), h = LbOr(l->box_height, 770.0f);
    float gap = LbOr(l->gap, 14.0f);
    uint32_t total = ours ? 3 + g_lbCount : 3, columns = l->columns >= 3 && l->columns < total ? l->columns : total;
    uint32_t rows = (total + columns - 1) / columns;
    float rowW = wide + (float)(columns - 1) * (w + gap), rowH = (float)rows * h + (float)(rows - 1) * gap;
    LB_CALL(LbVecFn, 0x1447160u)(o + LB_O_ROW, LbXY(rowW, rowH));
    LB_CALL(LbVecFn, 0x1447160u)(o + LB_O_FRAME, LbXY(rowW > 1520.0f ? rowW : 1520.0f, rowH));
    if (g_lbLayout.box_width <= 0.0f && g_lbLayout.box_height <= 0.0f) return;
    for (uint32_t i = 0; i < total; i++)
    {
        uintptr_t box = i < 3 ? LbGameBox(o, i) : LbBoxAt(i - 3);
        LB_CALL(LbVecFn, 0x1447160u)(box, LbXY(w, h));
        LB_CALL(LbVecFn, 0x1447160u)(box + 0x110u, LbXY(w, h));
    }
}

static uintptr_t LbRenderer(void)
{
    uintptr_t r = 0;
    return EvpPtr(g_game + LB_RENDERER, &r) ? r : 0;
}

/* Our preview slot given back and our cells emptied, as the game does with its own when the overview hides. */
static void LbFreeSlot(void)
{
    uintptr_t r = LbRenderer();
    if (g_lbSlot != LB_NONE && r) LB_CALL(LbFreeFn, 0x13918E0u)(r, g_lbSlot);
    g_lbSlot = LB_NONE;
    for (uint32_t k = 0; k < LB_BOXES; k++)
    {
        memset(g_lbCells[k], 0, LB_CELL);
        memset(g_lbCells[k] + 0x3C, 0xFF, 8);
        g_lbCells[k][0x48] = 2;
    }
}

/* A free preview slot taken as the game's set-up takes one: marked in use, its cell marks cleared. */
static uint32_t LbTakeSlot(void)
{
    LbFreeSlot();
    uintptr_t r = LbRenderer();
    for (uint32_t i = 0; r && i < LB_SLOTS; i++)
    {
        uint32_t* state = (uint32_t*)(r + 0x728u + i * 0x718u);
        if (*state != 0) continue;
        *state = 1;
        memset((void*)(r + 0x2AB0u + i * 0xFu), 0, 0xF);
        return g_lbSlot = i;
    }
    return LB_NONE;
}

/* The slot our boxes' weapons render in while they show; none on APPAREL. */
static uint32_t LbSlotFor(int shown)
{
    if (!shown) { LbFreeSlot(); return LB_NONE; }
    uint32_t slot = LbTakeSlot();
    if (slot == LB_NONE) Note("loadout boxes: no free preview slot; the boxes show no weapon");
    else Note("loadout boxes: preview slot %u", slot);
    return slot;
}

/* One of our boxes set up as the game's set-up does a weapon box: id, label, the item and its preview cell. */
static void LbSetUpBox(uintptr_t o, uint32_t k, uint32_t slot)
{
    uintptr_t box = LbBoxAt(k);
    LbBox b = g_lbBoxes[k];
    uint32_t item = b.itemFn ? b.itemFn(b.mod, b.handle, b.user) : 0, id = 0;
    *(uint32_t*)(box + LB_BOX_TYPE) = 0x100u + k; /* no game type: 0x14570B0 and the grid never see it */
    memcpy(&id, (void*)(box + LB_BOX_ID), 4);
    LB_CALL(LbOFn, 0x1446D10u)(box);
    LB_CALL(LbRegisterFn, 0x14509E0u)(box, id + LB_UI_ID, 0, 0);
    LB_CALL(LbLabelFn, 0x18DE230u)(box + 0x220u, b.text, 0);
    uint32_t* art = (uint32_t*)(box + LB_BOX_ITEM);
    if (art[4] != LB_KIND) { art[4] = LB_KIND; *(uint8_t*)(box + LB_BOX_ITEM + 0x15u) = 1; }
    art[0] = item;
    int shown = 0;
    LB_CALL(LbArtFn, 0x18DF2F0u)(box + 0x9C0u, &shown);
    art[1] = shown ? (uint32_t)shown : item;
    _Alignas(16) uint8_t desc[0x50] = {0};
    uint32_t place[4] = {0, k, 2, 1}; /* row k, the whole width: column 0, 2 columns, 1 row, as the game places */
    memcpy(desc + 0x3C, place, sizeof place);
    memset(g_lbCells[k], 0, LB_CELL);
    if (slot == LB_NONE) return;
    LB_CALL(LbCellFn, 0x178E170u)(item, slot, desc, (uintptr_t)g_lbCells[k], 0);
    LB_CALL(LbBindFn, 0x18E10B0u)(box, (uintptr_t)g_lbCells[k]);
}

static void LbSetUp(uintptr_t o)
{
    uint32_t slot = *(uint32_t*)(o + LB_O_SLOT), mode = *(uint32_t*)(o + LB_O_MODE);
    g_lbShown = o == g_lbO && mode == 0 && slot != LB_NONE;
    if (o == g_lbO) slot = LbSlotFor(g_lbShown);
    for (uint32_t k = 0; k < g_lbCount && o == g_lbO; k++)
    {
        LB_CALL(LbAlphaFn, 0x1448AD0u)(LbBoxAt(k), g_lbShown ? 1.0f : 0.0f);
        if (g_lbShown) LbSetUpBox(o, k, slot);
    }
    /* The renderer draws a slot only once it is queued, as the game's set-up queues its own after placing. */
    if (g_lbShown && slot != LB_NONE && g_lbCount) LB_CALL(LbSubmitFn, 0x1391890u)(0, slot);
    if (o == g_lbO) LbLayoutSizes(o, g_lbShown);
    if (!g_lbShown) g_lbSel = -1;
    else if (g_lbSel >= 0)
    {
        g_lbSelect(o, 0xFFFFFFFFu, 0); /* the set-up picked a game box again */
        LB_CALL(LbHighlightFn, LB_HIGHLIGHT)(LbBoxAt((uint32_t)g_lbSel), 1);
    }
}

/* Our box k selected (the game's set to none), or ours cleared (k -1) when the game selects one of its own. */
static void LbPick(uintptr_t o, int k, int sound)
{
    if (k == g_lbSel) return;
    if (g_lbSel >= 0) LB_CALL(LbHighlightFn, LB_HIGHLIGHT)(LbBoxAt((uint32_t)g_lbSel), 0);
    g_lbSel = k;
    if (k < 0) return;
    g_lbSelect(o, 0xFFFFFFFFu, 0);
    LB_CALL(LbHighlightFn, LB_HIGHLIGHT)(LbBoxAt((uint32_t)k), 1);
    LB_CALL(LbOFn, 0x18E1600u)(LbBoxAt((uint32_t)k));
    if (sound) LB_CALL(LbSoundFn, 0x1327F50u)(0, LB_SOUND_MOVE);
}

/* The bar's index of a mod tab on the WEAPONRY page, -1 when it is not shown there. */
static int LbTabIndex(uintptr_t bar, uint32_t handle)
{
    uint32_t count, ids[AT_LABELS];
    AtTab tabs[AT_LABELS];
    int list = AtShown(AT_ARMORY, bar, &count, ids);
    if (list < 0 || kAtLists[list].tabs != HF_MENU_TABS_ARMORY_WEAPONS) return -1;
    uint32_t n = AtTabsOf(list, tabs);
    for (uint32_t i = 0; i < n; i++)
        if (tabs[i].handle == handle && kAtLists[list].count + i < count) return (int)(kAtLists[list].count + i);
    return -1;
}

/* A click or confirm on our box: the weapon page opens as for PRIMARY, then the box's tab is picked on the bar, as a
 * click on its label would, so the mod's on_select builds the page. */
static void LbOpen(uintptr_t o, uint32_t k)
{
    uintptr_t screen = o - LB_SCREEN_O, bar = screen + kAtBars[AT_ARMORY].bar;
    *(uint32_t*)(o + LB_O_SELECTED) = 0;
    ((AtOpenFn)(g_atGame + AT_ARMORY_OPEN))(screen, 1);
    int index = LbTabIndex(bar, g_lbBoxes[k].tab);
    if (index < 0) { Note("loadout boxes: box %u: its tab %u is not on the WEAPONRY bar", k, g_lbBoxes[k].tab); return; }
    ((AtPickFn)(g_atGame + AT_PICK))(bar, (uint32_t)index, 1, 1);
    AtFollow(AT_ARMORY, bar);
}

static int LbHit(uintptr_t cursor)
{
    for (uint32_t k = 0; k < g_lbCount; k++)
        if (LB_CALL(LbHitFn, 0x178F970u)(cursor, LbBoxAt(k), 0)) return (int)k;
    return -1;
}

static int LbMouse(void)
{
    uintptr_t at = 0;
    return EvpPtr(g_game + LB_MOUSE, &at) && at && *(const uint8_t*)(at + 0xE1AD4u) != 0;
}

static void LbConstructDetour(uintptr_t o, uint64_t position, uint64_t anchor, uint64_t pivot)
{
    g_lbConstruct(o, position, anchor, pivot);
    if (g_lbCount) LbBuild(o);
}

static void LbSetupDetour(uintptr_t o)
{
    g_lbSetup(o);
    if (g_lbCount) LbSetUp(o);
}

/* The game's per-frame box loop, for ours, and the mouse over them (the game tests only its frame's area). */
static void LbUpdateDetour(uintptr_t screen, float dt)
{
    g_lbUpdate(screen, dt);
    uintptr_t o = screen + LB_SCREEN_O, menu = 0;
    if (o != g_lbO) /* the screen was built before HellForge's hooks were placed */
    {
        LbBuild(o);
        if (!*(const uint8_t*)(o + LB_O_HIDDEN)) LbSetUp(o);
    }
    if (!g_lbShown || o != g_lbO || *(const uint8_t*)(o + LB_O_HIDDEN)) return;
    for (uint32_t k = 0; k < g_lbCount; k++) LB_CALL(LbFrameFn, 0x18DC5D0u)(LbBoxAt(k) + LB_BOX_TRANSFORM, dt);
    if (!LbMouse() || !EvpPtr(g_game + LB_MENU, &menu) || !menu) return;
    int k = LbHit(menu + 0x5F0u);
    if (k >= 0) LbPick(o, k, 1);
}

/* Moves and confirm while the game's box is selected: right from THROWABLE, or a click on one of ours. */
static uint64_t LbNavGame(uintptr_t o, uintptr_t input, uintptr_t cursor, uint32_t dir, int confirm)
{
    if (*(const uint32_t*)(o + LB_O_SELECTED) == 2 && dir == 4) { LbPick(o, 0, 1); return 1; }
    int hit = confirm && LbMouse() ? LbHit(cursor) : -1;
    if (hit < 0) return g_lbNav(o, input, cursor);
    LbPick(o, hit, 0);
    LbOpen(o, (uint32_t)hit);
    return 0;
}

/* Left and right across the game's boxes and ours, and confirm on ours. */
static uint64_t LbNavDetour(uintptr_t o, uintptr_t input, uintptr_t cursor)
{
    if (!g_lbShown || o != g_lbO) return g_lbNav(o, input, cursor);
    uint32_t dir = LB_CALL(LbDirFn, 0x178E020u)(input), action = LB_CALL(LbActionFn, 0x585B80u)(0xA00000000ull);
    int confirm = *(const uint8_t*)(input + 0x328u + (uintptr_t)action * 0x20u) != 0;
    if (g_lbSel < 0) return LbNavGame(o, input, cursor, dir, confirm);
    if (dir == 3 && g_lbSel == 0) { LbPick(o, -1, 0); g_lbSelect(o, 2, 1); return 1; }
    if (dir == 3) { LbPick(o, g_lbSel - 1, 1); return 1; }
    if (dir == 4) { if ((uint32_t)g_lbSel + 1 < g_lbCount) LbPick(o, g_lbSel + 1, 1); return 1; }
    if (confirm && (!LbMouse() || LbHit(cursor) == g_lbSel)) LbOpen(o, (uint32_t)g_lbSel);
    return 0;
}

static void LbSelectDetour(uintptr_t o, uint32_t index, uint8_t sound)
{
    g_lbSelect(o, index, sound);
    if (index != 0xFFFFFFFFu && o == g_lbO && g_lbSel >= 0) LbPick(o, -1, 0);
}

/* Hidden, the overview gives its preview slot back, and ours goes back too. */
static void LbHideDetour(uintptr_t o, uint8_t hide)
{
    uint8_t was = *(const uint8_t*)(o + LB_O_HIDDEN);
    g_lbHide(o, hide);
    if (!hide || was || o != g_lbO || !g_lbShown) return;
    for (uint32_t k = 0; k < g_lbCount; k++)
    {
        uintptr_t box = LbBoxAt(k);
        LB_CALL(LbOFn, 0x1943710u)(box + LB_BOX_TRANSFORM);
        *(uintptr_t*)(box + LB_BOX_CELL) = box + LB_BOX_CELL + 0x18u;
    }
    LbFreeSlot();
    g_lbShown = 0;
}

/* The highlight animates a box from 326 to 500 wide and back; set_loadout_layout's widths take their place. */
static void LbHighlightDetour(uintptr_t box, uint8_t on)
{
    g_lbAnimating = box;
    g_lbHighlight(box, on);
    g_lbAnimating = 0;
}

static uint64_t LbWidth(uint64_t xy)
{
    float v[2];
    memcpy(v, &xy, 8);
    if (v[0] == 326.0f) v[0] = LbOr(g_lbLayout.box_width, 326.0f);
    else if (v[0] == 500.0f) v[0] = LbOr(g_lbLayout.wide_width, 500.0f);
    return LbXY(v[0], v[1]);
}

static void LbAnimateDetour(uintptr_t widget, uint64_t from, uint64_t to, uint8_t on, uint32_t a, uint32_t b, void* c)
{
    if (g_lbShown && g_lbAnimating && widget >= g_lbAnimating && widget < g_lbAnimating + LB_BOX) from = LbWidth(from), to = LbWidth(to);
    g_lbAnimate(widget, from, to, on, a, b, c);
}

static int LbHookAll(void)
{
    int widths = g_lbLayout.box_width > 0.0f || g_lbLayout.wide_width > 0.0f;
    return LoaderHook("loadout boxes: build", g_game + LB_CONSTRUCT, (void*)LbConstructDetour, (void**)&g_lbConstruct) ||
           LoaderHook("loadout boxes: set-up", g_game + LB_SETUP, (void*)LbSetupDetour, (void**)&g_lbSetup) ||
           LoaderHook("loadout boxes: update", g_game + LB_UPDATE, (void*)LbUpdateDetour, (void**)&g_lbUpdate) ||
           LoaderHook("loadout boxes: moves", g_game + LB_NAV, (void*)LbNavDetour, (void**)&g_lbNav) ||
           LoaderHook("loadout boxes: select", g_game + LB_SELECT, (void*)LbSelectDetour, (void**)&g_lbSelect) ||
           LoaderHook("loadout boxes: hide", g_game + LB_HIDE, (void*)LbHideDetour, (void**)&g_lbHide) ||
           (widths && LoaderHook("loadout boxes: highlight", g_game + LB_HIGHLIGHT, (void*)LbHighlightDetour, (void**)&g_lbHighlight)) ||
           (widths && LoaderHook("loadout boxes: widths", g_game + LB_ANIMATE, (void*)LbAnimateDetour, (void**)&g_lbAnimate));
}

/* Placed before the Armory screen is built (it is built once, when the game has loaded), so our boxes join it. */
static void LbInstall(void)
{
    const char* why = NULL;
    if (!g_game || strcmp(g_build, "25480438")) why = "unsupported game build";
    else if (g_atState < 0) why = "menu tabs are off";
    else if (!ActPins(g_game, kLbPins, sizeof kLbPins / sizeof kLbPins[0])) why = "the overview's code is not the reviewed one";
    else if (LbHookAll()) why = "a hook could not be placed";
    g_lbState = why ? -1 : 1;
    if (why) Note("loadout boxes: off: %s; the mods' WEAPONRY boxes are not shown", why);
    else Note("loadout boxes: on, %u boxes", g_lbCount);
}

static void LbTick(HfMod* mod, float dt, void* user)
{
    (void)mod, (void)dt, (void)user;
    if (g_lbState == 0 && g_atState != 0) LbInstall();
}

/* SDK 42, add_loadout_box: the box's handle (1 and up), or 0 refused (logged). */
static uint32_t ApiAddLoadoutBox(HfMod* mod, const HfLoadoutBoxDef* def)
{
    if (mod == NULL || def == NULL || def->size < sizeof(HfLoadoutBoxDef) || def->label == NULL || def->label[0] == 0 || def->tab == 0)
    { ApiLog(mod, "add_loadout_box: refused: it needs its size, a label and an add_armory_tab handle"); return 0; }
    if (g_lbState != 0 || g_fired[HF_EVENT_GAME_LOADED]) { ApiLog(mod, "add_loadout_box: too late; call it from hellforge_mod_init"); return 0; }
    char key[96];
    snprintf(key, sizeof key, "loadout_box/%s", def->label);
    uint32_t text = ApiAddText(mod, key, def->label), handle = 0;
    if (text == 0) return 0;
    AcquireSRWLockExclusive(&g_lbLock);
    if (g_lbCount < LB_BOXES)
    {
        handle = g_lbCount + 1;
        g_lbBoxes[g_lbCount++] = (LbBox){mod, def->item_fn, def->user, def->tab, text, handle};
    }
    ReleaseSRWLockExclusive(&g_lbLock);
    if (handle == 0) { ApiLog(mod, "add_loadout_box(%s): refused: mods have all %u boxes", def->label, LB_BOXES); return 0; }
    if (handle == 1)
    {
        g_lbMod.api = &g_api;
        snprintf(g_lbMod.id, sizeof g_lbMod.id, "hellforge.loadout-boxes");
        g_lbMod.log = g_log;
        g_lbMod.loaded = 1;
        ApiOnGameThread(&g_lbMod, LbTick, NULL);
    }
    ApiLog(mod, "add_loadout_box(%s): box %u, opens tab %u", def->label, handle, def->tab);
    return handle;
}

/* SDK 42, set_loadout_layout: 0 done, 1 refused (logged). */
static int ApiSetLoadoutLayout(HfMod* mod, const HfLoadoutLayout* layout)
{
    if (mod == NULL || layout == NULL || layout->size < sizeof(HfLoadoutLayout) || (layout->columns && (layout->columns < 3 || layout->columns > 6)))
    { ApiLog(mod, "set_loadout_layout: refused: it needs its size, and columns 0 or 3 to 6"); return 1; }
    if (g_lbState != 0 || g_fired[HF_EVENT_GAME_LOADED]) { ApiLog(mod, "set_loadout_layout: too late; call it from hellforge_mod_init"); return 1; }
    g_lbLayout = *layout;
    ApiLog(mod, "set_loadout_layout: %u columns, boxes %.0f x %.0f (selected %.0f wide), gap %.0f", layout->columns,
           layout->box_width, layout->box_height, layout->wide_width, layout->gap);
    return 0;
}

#define LB_API ApiAddLoadoutBox, ApiSetLoadoutLayout
