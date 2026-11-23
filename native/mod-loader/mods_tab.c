/* HellForge's own MODS tab on the escape menu, listing the SDK 30 options with the game's own widgets (build 25480438).
 * A port of the Vanilla Plus Megapack's Mod Options Menu (CowboyBingus, 0BSD), whose review of each offset and function
 * it keeps: the bar's label setter is called, never hooked, and every row names setting 139, whose value type is none,
 * so no game setting is read or written. It steps aside while the Megapack's own menu is on, since that menu already
 * shows these options through the bundled Lua mod. Layout: docs/internals-world.md "The escape menu's MODS tab". */

/* ---- the game ---------------------------------------------------------------------------------------------------- */

#define MT_UI_STATE 0x347CE28u      /* game.dll: the UI state, its screen stack at +0x429C (5 types, then the count) */
#define MT_UI_STACK 0x429Cu
#define MT_UI_ENTRIES 5u
#define MT_MENU 0x347CE38u          /* game.dll: the menu object; the escape menu's screen at +200 */
#define MT_ESCAPE_SCREEN 200u
#define MT_ESCAPE_TYPE 1u
#define MT_INPUT_OWNER 0x347CF18u   /* game.dll: the input owner (this frame's actions) */
#define MT_TAB_LABELS_RVA 0x33114D0u
#define MT_CATEGORY_TABLE_RVA 0x32EC990u
#define MT_CATEGORY_RECORD 48u
#define MT_INERT_DESCRIPTOR_RVA 0x32ECC60u
#define MT_INERT_SETTING 139u
#define MT_INERT_CATEGORY 10u
#define MT_TEXT_TEMPLATE 0xC67C7FAFu /* '#COUNT': a pure tag template, so a string COUNT argument shows any text */
#define MT_TEXT_KEY 0xAB2A7B35u      /* MurmurHash64A("COUNT") >> 32 */

/* The escape menu screen. */
#define MT_SHOWN_CONTENT 8u
#define MT_TAB_BAR 1248u
#define MT_OPTIONS_CONTENT 3010456u
/* Its tab bar: eight buttons; the screen asks for three. */
#define MT_TAB_COUNT 57448u
#define MT_TAB_IDS 57320u
#define MT_TAB_CURRENT 57452u
#define MT_TAB_BUTTON_STATE 11004u
#define MT_TAB_BUTTON_ACTIVE 11021u
#define MT_TAB_TEXT 8296u
#define MT_TAB_STRIDE 3400u
#define MT_NATIVE_TABS 3u
#define MT_OPTIONS_TAB 2u
#define MT_MODS_TAB 3u
/* The OPTIONS content: nine category buttons, one panel per category, a description box, the dialog. */
#define MT_CURRENT_CATEGORY 1318488u
#define MT_PREVIOUS_CATEGORY 1318492u
#define MT_CATEGORY_BUTTON 816u
#define MT_CATEGORY_STRIDE 14920u
#define MT_CATEGORY_TEXT 1928u
#define MT_CATEGORIES 9u
#define MT_BUTTONS 8
#define MT_PAGE_MODS 7
#define MT_DESCRIPTION_BOX 1315800u
#define MT_DESCRIPTION_FRAME 272u
#define MT_DESCRIPTION_TITLE 936u
#define MT_DESCRIPTION_BODY 1632u
#define MT_DESCRIBED_SETTING 1318500u
#define MT_SHOWN_SETTING 1318476u
#define MT_NO_SETTING 156u
#define MT_WIDGET_HEIGHT 16u
#define MT_WIDGET_SCALE_Y 32u
#define MT_LABEL 272u              /* a text widget's label id; up to 14 arguments {key, type, value} from +280 */
#define MT_ARGUMENTS 280u
#define MT_ARGUMENT_COUNT 616u
#define MT_STRING_ARGUMENT 1u
/* Apply and the UNAPPLIED CHANGES dialog. */
#define MT_UNAPPLIED 1319449u
#define MT_DIALOG 1296040u
#define MT_DIALOG_MODE 19748u
#define MT_DIALOG_STATE 19752u
#define MT_DIALOG_CONFIRM 4608u
#define MT_DIALOG_CONFIRM_ACTION 12072u
#define MT_DIALOG_CANCEL 12160u
#define MT_DIALOG_CANCEL_ACTION 19624u
#define MT_DIALOG_OPEN 2u
#define MT_UNAPPLIED_MODE 0u
#define MT_NEUTRAL_MODE 2u         /* a mode the game's confirm ignores: this tab discards itself */
#define MT_VISIBLE_FLAG 0x10u
#define MT_ACTION_STATES 808u
#define MT_ACTION_STRIDE 32u
#define MT_GROUP_ACTIONS 97u
#define MT_ACTION_GROUPS 13u
#define MT_MENU_GROUP 0u
#define MT_APPLY_ACTION 12u        /* Menu.ExtraOption1 (Tab) */
#define MT_APPLY_SOUND 1183616600u
/* Panels and rows. */
#define MT_ROW_ARRAY 144000u
#define MT_PANEL_ROWS 2816u
#define MT_PANEL_CONTAINER 544u
#define MT_PANEL_SCROLL 816u
#define MT_SCROLL_POSITION 1976u
#define MT_VIEW_HEIGHT 806.0
#define MT_ROW_HEIGHT 64.0
#define MT_GAP_HEIGHT 32.0
#define MT_MAX_ROWS 32
#define MT_ROW_STRIDE 31464u
#define MT_ROW_TEXT 3992u
#define MT_ROW_SELECTOR 16016u
#define MT_ROW_VALUE_TEXT 16832u
#define MT_ROW_INDEX 29068u
#define MT_ROW_SLIDER 29176u
#define MT_ROW_SLIDER_VALUE 31408u
#define MT_ROW_SETTING 31428u
#define MT_ROW_CHOICE_COUNT 29072u
#define MT_ROW_CHOICES 29080u

enum { MT_FAMILY_SIMPLE, MT_FAMILY_LARGE, MT_FAMILY_ACCESS };
typedef struct MtPanel { uint32_t offset; int family; uint32_t selected; } MtPanel;
static const MtPanel kMtPanels[8] = {
    {1150848u, MT_FAMILY_SIMPLE, 2872u}, {1179528u, MT_FAMILY_LARGE, 33296u}, {1253248u, MT_FAMILY_LARGE, 33296u},
    {1287376u, MT_FAMILY_SIMPLE, 2872u}, {1153728u, MT_FAMILY_SIMPLE, 2872u}, {1156616u, MT_FAMILY_ACCESS, 22392u},
    {1290256u, MT_FAMILY_SIMPLE, 2880u}, {1293152u, MT_FAMILY_SIMPLE, 2872u}};
typedef struct MtFamily { uint32_t count, x, gap, height, settings, scrolls; } MtFamily; /* height 0: none */
static const MtFamily kMtFamilies[3] = {
    {2832u, 2836u, 2840u, 2844u, 2856u, 2830u}, {33256u, 33260u, 33264u, 33268u, 33280u, 33254u},
    {22352u, 22356u, 22360u, 0u, 22376u, 22350u}};

static const uint32_t kMtTabLabels[3] = {0xD876B36Eu, 0x78934E12u, 0x8C02BD80u}; /* GAME, SOCIAL, OPTIONS */
static const uint32_t kMtCategoryLabels[9] = {0x396F27D7u, 0x196248D0u, 0xA522D2D0u, 0x15CECAC1u, 0x9A688238u,
    0x07E3B100u, 0x5DB3B764u, 0xA7C345EFu, 0x9BB47B92u}; /* GAMEPLAY ... ACCOUNT */

/* ---- game.dll's functions ------------------------------------------------------------------------------------------
 * Plain Microsoft x64 functions (the Megapack's native.h has each one's review); a {float, float} goes by value in one
 * integer register. */

typedef struct MtVec2 { float x, y; } MtVec2;
typedef struct MtNative
{
    void (*set_tab_labels)(uintptr_t bar, const uint32_t* labels, int count);
    void (*select_category)(uintptr_t content, int category);
    void (*set_content_hidden)(uintptr_t content, uint8_t hidden, uint8_t immediate);
    void (*row_reset)(uintptr_t row, const float* first, const float* second);
    void (*row_release)(uintptr_t row);
    void (*row_init)(uintptr_t row, MtVec2 position, MtVec2 pivot, MtVec2 anchor, int layer, uintptr_t settings,
        const void* descriptor, uint8_t flag);
    void (*add_child)(uintptr_t parent, uintptr_t child);
    void (*set_visible)(uintptr_t widget, uint8_t visible);
    void (*set_label)(uintptr_t widget, uint32_t label);
    void (*set_string_arg)(uintptr_t widget, uint32_t key, const char* text);
    uint8_t (*clear_args)(uintptr_t label);
    void (*set_choice)(uintptr_t selector, uint32_t index);
    void (*set_slider)(uintptr_t slider, float value);
    void (*finish_simple)(uintptr_t panel);
    void (*finish_access)(uintptr_t panel);
    void (*scroll_thumb)(uintptr_t scroll, float size);
    void (*scroll_extent)(uintptr_t scroll, float extent);
    void (*set_opacity)(uintptr_t widget, float opacity);
    void (*scroll_reset)(uintptr_t scroll);
    void (*options_visual)(uintptr_t content, float dt);
    void (*measure_text)(uintptr_t widget);
    void (*set_position)(uintptr_t widget, MtVec2 position);
    void (*set_size)(uintptr_t widget, MtVec2 size);
    void (*hide_description)(uintptr_t box, uint8_t hidden, uint8_t animate);
    void (*play_sound)(uintptr_t unused, uint32_t event);
} MtNative;
static MtNative g_mt;

typedef struct MtPin { uint32_t rva; const char* bytes; size_t field; } MtPin;
#define MT_FIELD(name) offsetof(MtNative, name)
static const MtPin kMtPins[] = {
    {0x17AAC50u, "48 89 54 24 10 53 56 48 83 EC 68 0F 29 74 24 30", MT_FIELD(set_tab_labels)},
    {0x19999C0u, "40 53 48 83 EC 20 48 8B D9 89 91 58 1E 14 00 39", MT_FIELD(select_category)},
    {0x19984C0u, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 50 41", MT_FIELD(set_content_hidden)},
    {0x17FF700u, "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55", MT_FIELD(row_reset)},
    {0x17FF4B0u, "48 89 5C 24 08 57 48 83 EC 20 8B 81 C0 7A 00 00", MT_FIELD(row_release)},
    {0x17FE8D0u, "48 89 5C 24 08 57 48 83 EC 70 48 8B BC 24 B0 00", MT_FIELD(row_init)},
    {0x144C5C0u, "40 53 48 83 EC 20 4C 8B CA 48 8B D9 48 8B 91 E0", MT_FIELD(add_child)},
    {0x144CFB0u, "48 83 EC 28 44 8B 01 4C 8B D1 41 8B C0 45 8B C8", MT_FIELD(set_visible)},
    {0x143BF90u, "48 83 EC 28 4C 8B D9 39 91 10 01 00 00 0F 84 80", MT_FIELD(set_label)},
    {0x143C950u, "40 53 48 83 EC 20 48 8B D9 48 81 C1 10 01 00 00", MT_FIELD(set_string_arg)},
    {0x143A0F0u, "80 B9 58 01 00 00 00 C6 81 58 01 00 00 00 0F 97", MT_FIELD(clear_args)},
    {0x17FE7E0u, "80 B9 F8 32 00 00 00 74 28 83 FA 10 73 23 48 63", MT_FIELD(set_choice)},
    {0x17FC2C0u, "40 53 48 83 EC 20 F3 0F 10 91 AC 08 00 00 48 8B", MT_FIELD(set_slider)},
    {0x1807060u, "48 89 5C 24 08 57 48 83 EC 30 8B 81 10 0B 00 00", MT_FIELD(finish_simple)},
    {0x1800FE0u, "48 89 5C 24 08 57 48 83 EC 30 8B 81 50 57 00 00", MT_FIELD(finish_access)},
    {0x1793F60u, "48 89 5C 24 10 57 48 83 EC 40 48 8B D9 0F 29 74", MT_FIELD(scroll_thumb)},
    {0x1794600u, "F3 0F 10 A1 B0 07 00 00 0F 28 D1 0F 2E E2 7A 02", MT_FIELD(scroll_extent)},
    {0x1448AD0u, "40 57 48 83 EC 20 F3 0F 10 41 44 48 8B F9 0F 2E", MT_FIELD(set_opacity)},
    {0x1794570u, "80 B9 B5 07 00 00 00 74 19 F3 0F 10 89 BC 07 00", MT_FIELD(scroll_reset)},
    {0x1998590u, "48 8B C4 57 48 83 EC 70 80 B9 15 22 14 00 00 48", MT_FIELD(options_visual)},
    {0x144E1A0u, "40 53 48 83 EC 70 48 8B D9 8B 09 8B C1 C1 E8 12", MT_FIELD(measure_text)},
    /* Position and size share their first 27 bytes; byte 28 picks the field. */
    {0x14476A0u, "48 89 5C 24 18 48 89 6C 24 20 48 89 54 24 10 56 57 41 57 48 83 EC 20 F3 0F 10 41 04", MT_FIELD(set_position)},
    {0x1447160u, "48 89 5C 24 18 48 89 6C 24 20 48 89 54 24 10 56 57 41 57 48 83 EC 20 F3 0F 10 41 0C", MT_FIELD(set_size)},
    {0x17FAB90u, "48 89 5C 24 10 57 48 83 EC 70 0F B6 DA 48 8B F9", MT_FIELD(hide_description)},
    {0x1327F50u, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48", MT_FIELD(play_sound)},
};
#define MT_PINS (sizeof kMtPins / sizeof kMtPins[0])

/* Plain loads and stores on the escape menu's screen, which the game uses while it is on top of the UI stack: the frames
 * this tab reads and writes it (the idle gate checks it first through guarded reads). */
static inline uint8_t MtGet8(uintptr_t at) { uint8_t v; memcpy(&v, (const void*)at, 1); return v; }
static inline uint32_t MtGet32(uintptr_t at) { uint32_t v; memcpy(&v, (const void*)at, 4); return v; }
static inline float MtGetF(uintptr_t at) { float v; memcpy(&v, (const void*)at, 4); return v; }
static inline uint64_t MtGet64(uintptr_t at) { uint64_t v; memcpy(&v, (const void*)at, 8); return v; }
static inline void MtPut8(uintptr_t at, uint8_t v) { memcpy((void*)at, &v, 1); }
static inline void MtPut32(uintptr_t at, uint32_t v) { memcpy((void*)at, &v, 4); }
static inline void MtPutF(uintptr_t at, float v) { memcpy((void*)at, &v, 4); }
static int MtPointer(uint64_t v) { return v >= 0x10000u && v < 0x800000000000ull; }

#include "mods_tab_model.c" /* its texts, the model of what it lists, and the values */

/* ---- widgets ------------------------------------------------------------------------------------------------------- */

static uintptr_t MtTextArgument(uintptr_t widget)
{
    uint32_t count = MtGet8(widget + MT_ARGUMENT_COUNT);
    if (count > 14) count = 14;
    for (uint32_t i = 0; i < count; i++)
    {
        uintptr_t record = widget + MT_ARGUMENTS + 24u * i;
        if (MtGet32(record) == MT_TEXT_KEY) return record;
    }
    return 0;
}

static int MtShowsText(uintptr_t widget, const char* text)
{
    if (MtGet32(widget + MT_LABEL) != MT_TEXT_TEMPLATE) return 0;
    uintptr_t record = MtTextArgument(widget);
    if (record == 0 || MtGet32(record + 4) != MT_STRING_ARGUMENT) return 0;
    return text == NULL ? 1 : MtGet64(record + 8) == (uint64_t)(uintptr_t)text;
}

static void MtShowText(uintptr_t widget, const char* text)
{
    g_mt.set_label(widget, MT_TEXT_TEMPLATE);
    g_mt.set_string_arg(widget, MT_TEXT_KEY, text);
}

static void MtShowLabel(uintptr_t widget, uint32_t label)
{
    (void)g_mt.clear_args(widget + MT_LABEL);
    g_mt.set_label(widget, label);
}

/* A fourth tab on the bar that is not this one: another mod's MODS tab (the Megapack's, started later or live). */
static int MtForeignTab(uintptr_t bar)
{
    if (MtGet32(bar + MT_TAB_COUNT) != MT_NATIVE_TABS + 1) return 0;
    uintptr_t title = bar + MT_TAB_TEXT + MT_TAB_STRIDE * MT_MODS_TAB;
    return MtGet32(bar + MT_TAB_IDS + 4u * MT_MODS_TAB) != MT_TEXT_TEMPLATE || !MtShowsText(title, g_mtab.title);
}

/* MODS as the bar's fourth tab: the game draws it and cycles Q/E over it. Relabelling resets every button's state, so the
 * current tab's selected state is put back as the screen's opener does. 0 when the bar is not the one reviewed. */
static int MtEnsureTab(uintptr_t screen)
{
    uintptr_t bar = screen + MT_TAB_BAR;
    uint32_t count = MtGet32(bar + MT_TAB_COUNT);
    if (count == MT_NATIVE_TABS + 1) return !MtForeignTab(bar);
    if (count != MT_NATIVE_TABS) return 0;
    uint32_t current = MtGet32(bar + MT_TAB_CURRENT), labels[MT_NATIVE_TABS + 1];
    if (current >= MT_NATIVE_TABS) return 0;
    for (uint32_t i = 0; i < MT_NATIVE_TABS; i++)
        if ((labels[i] = MtGet32(bar + MT_TAB_IDS + 4u * i)) != kMtTabLabels[i]) return 0;
    labels[MT_MODS_TAB] = MT_TEXT_TEMPLATE;
    g_mt.set_tab_labels(bar, labels, (int)MT_NATIVE_TABS + 1);
    uintptr_t button = bar + MT_TAB_STRIDE * current;
    MtPut32(button + MT_TAB_BUTTON_STATE, 3);
    MtPut8(button + MT_TAB_BUTTON_ACTIVE, 1);
    MtShowText(bar + MT_TAB_TEXT + MT_TAB_STRIDE * MT_MODS_TAB, g_mtab.title);
    return 1;
}

static uintptr_t MtCategoryButton(uintptr_t content, uint32_t index) { return content + MT_CATEGORY_BUTTON + MT_CATEGORY_STRIDE * index; }

/* Each button shows its category (or the page control), button 0 the empty text when there is none; the rest hide. */
static void MtLabelCategories(const MtView* view)
{
    for (uint32_t i = 0; i < MT_CATEGORIES; i++)
    {
        uintptr_t button = MtCategoryButton(view->content, i);
        const MtCategory* category = i < MT_BUTTONS ? view->buttons[i] : NULL;
        if (category != NULL || (i == 0 && view->modCount == 0))
        {
            MtShowText(button + MT_CATEGORY_TEXT, category != NULL ? category->title : g_mtab.empty);
            g_mt.set_visible(button, 1);
        }
        else g_mt.set_visible(button, 0);
    }
}

static void MtRestoreCategories(uintptr_t content)
{
    for (uint32_t i = 0; i < MT_CATEGORIES; i++)
    {
        uintptr_t button = MtCategoryButton(content, i);
        MtShowLabel(button + MT_CATEGORY_TEXT, kMtCategoryLabels[i]);
        g_mt.set_visible(button, 1);
    }
}

static double MtTextHeight(uintptr_t widget) { return (double)MtGetF(widget + MT_WIDGET_HEIGHT) * (double)MtGetF(widget + MT_WIDGET_SCALE_Y); }

/* The entry's description laid out as the game lays out a setting's: title over body, the frame fitted to both. */
static void MtDescribe(MtView* view, int entry)
{
    uintptr_t box = view->content + MT_DESCRIPTION_BOX;
    const MtItem* item = entry >= 0 ? view->page.entries[entry].item : NULL;
    view->described = entry >= 0 ? (int64_t)view->page.generation * 64 + entry : MT_DESCRIBED_NONE;
    if (item == NULL || item->description == NULL) { g_mt.hide_description(box, 1, 1); return; }
    uintptr_t title = box + MT_DESCRIPTION_TITLE, body = box + MT_DESCRIPTION_BODY;
    MtShowText(title, item->label);
    MtShowText(body, item->description);
    g_mt.measure_text(body);
    g_mt.measure_text(title);
    double titleHeight = MtTextHeight(title), bodyHeight = MtTextHeight(body);
    g_mt.set_position(body, (MtVec2){14, (float)(-titleHeight - 24)});
    g_mt.set_opacity(body, 1);
    g_mt.set_size(box + MT_DESCRIPTION_FRAME, (MtVec2){450, (float)(bodyHeight + titleHeight + 7 + 72)});
    g_mt.hide_description(box, 0, 1);
}

/* The box back to the game: hidden at once, this tab's texts gone, both setting caches cleared so OPTIONS fills it. */
static void MtReleaseDescription(uintptr_t content)
{
    uintptr_t box = content + MT_DESCRIPTION_BOX;
    g_mt.hide_description(box, 1, 0);
    (void)g_mt.clear_args(box + MT_DESCRIPTION_TITLE + MT_LABEL);
    (void)g_mt.clear_args(box + MT_DESCRIPTION_BODY + MT_LABEL);
    MtPut32(content + MT_DESCRIBED_SETTING, MT_NO_SETTING);
    MtPut32(content + MT_SHOWN_SETTING, MT_NO_SETTING);
}

/* ---- rows and pages ------------------------------------------------------------------------------------------------ */

static const MtVec2 kMtPivot = {0.5f, 1.0f};
static _Alignas(16) const float kMtZeroRect[4] = {0, 0, 0, 0}; /* the reset loads both rectangles with movaps */

static double MtRowValue(const MtEntry* entry)
{
    if (entry->item->kind == HF_OPTION_SLIDER) return MtGetF(entry->row + MT_ROW_SLIDER_VALUE);
    return MtGet32(entry->row + MT_ROW_INDEX);
}

/* The value a row holds: 0 when the item cannot take it (the player's input never makes one; the game could). */
static int MtDecode(const MtItem* item, double raw, double* value)
{
    if (item->kind == HF_OPTION_TOGGLE) { *value = raw == 1 ? 1 : 0; return 1; }
    if (item->kind == HF_OPTION_CHOICE)
    {
        if (!(raw >= 0 && raw < item->choiceCount)) return 0;
        *value = raw;
        return 1;
    }
    return MtSnap(item, raw, value);
}

/* A choice's own word is the value widget's '#COUNT' argument. */
static void MtShowChoice(const MtEntry* entry)
{
    const MtItem* item = entry->item;
    if (item->kind != HF_OPTION_CHOICE) return;
    uint32_t index = MtGet32(entry->row + MT_ROW_INDEX);
    if (index < (uint32_t)item->choiceCount && item->labels[index] == MT_TEXT_TEMPLATE)
        MtShowText(entry->row + MT_ROW_VALUE_TEXT, item->choices[index]);
}

static void MtApplyValue(MtEntry* entry, double value)
{
    const MtItem* item = entry->item;
    if (item->kind == HF_OPTION_SLIDER) g_mt.set_slider(entry->row + MT_ROW_SLIDER, (float)value);
    else
    {
        double top = (double)(item->choiceCount - 1);
        uint32_t index = item->kind == HF_OPTION_TOGGLE ? (value != 0 ? 1u : 0u) : (uint32_t)(value < 0 ? 0 : value > top ? top : value);
        g_mt.set_choice(entry->row + MT_ROW_SELECTOR, index);
        MtShowChoice(entry);
    }
    entry->raw = MtRowValue(entry);
}

static void MtFinishLarge(uintptr_t panel, const MtFamily* family, uint32_t count, double height)
{
    uintptr_t scroll = panel + MT_PANEL_SCROLL;
    if (count == 0) { g_mt.set_opacity(scroll, 0); MtPut8(panel + family->scrolls, 0); return; }
    g_mt.scroll_thumb(scroll, (float)fmin(1, MT_VIEW_HEIGHT / height));
    g_mt.scroll_extent(scroll, (float)(height - MT_VIEW_HEIGHT));
    if (height > MT_VIEW_HEIGHT) { g_mt.set_opacity(scroll, 1); MtPut8(panel + family->scrolls, 1); return; }
    g_mt.set_opacity(scroll, 0);
    if (MtGetF(scroll + MT_SCROLL_POSITION) != 0) { MtPutF(scroll + MT_SCROLL_POSITION, 0); g_mt.scroll_reset(scroll); }
    MtPut8(panel + family->scrolls, 0);
}

static void MtFinishPanel(uintptr_t panel, int name)
{
    const MtFamily* family = &kMtFamilies[name];
    uint32_t count = MtGet32(panel + family->count);
    double height = count * MT_ROW_HEIGHT - MtGetF(panel + family->gap);
    if (family->height != 0) MtPutF(panel + family->height, (float)height);
    if (name == MT_FAMILY_SIMPLE) g_mt.finish_simple(panel);
    else if (name == MT_FAMILY_ACCESS) g_mt.finish_access(panel);
    else MtFinishLarge(panel, family, count, height);
}

/* The panel builders' sequence: every row released (last to first), then the category's rows from the item's
 * descriptor at the builders' positions and gaps, then the panel's own finishing layout. */
static int MtBuildPage(MtView* view, uint32_t index)
{
    if (index >= 8) return 0;
    const MtPanel* spec = &kMtPanels[index];
    const MtFamily* family = &kMtFamilies[spec->family];
    uintptr_t panel = view->content + spec->offset;
    uintptr_t rows = (uintptr_t)MtGet64(panel + MT_PANEL_ROWS);
    if (rows != view->content + MT_ROW_ARRAY) return 0;
    uintptr_t settings = (uintptr_t)MtGet64(panel + family->settings);
    if (!MtPointer(settings)) settings = 0;
    const MtCategory* category = index < MT_BUTTONS ? view->buttons[index] : NULL;
    MtPage* page = &view->page;
    memset(page, 0, sizeof *page);
    page->index = (int)index;
    page->count = category != NULL ? (category->count < MT_MAX_ROWS ? category->count : MT_MAX_ROWS) : 0;
    page->generation = ++g_mtab.generation;
    for (int i = MT_MAX_ROWS - 1; i >= 0; i--)
    {
        uintptr_t row = rows + MT_ROW_STRIDE * (uint32_t)i;
        g_mt.row_reset(row, kMtZeroRect, kMtZeroRect);
        g_mt.row_release(row);
    }
    double gap = 0;
    for (int position = 0; position < page->count; position++)
    {
        const MtItem* item = category->items[position];
        if (item->gap && position > 0) gap -= MT_GAP_HEIGHT;
        uintptr_t row = rows + MT_ROW_STRIDE * (uint32_t)position;
        g_mt.row_init(row, (MtVec2){0, (float)(position * -MT_ROW_HEIGHT + gap)}, kMtPivot, kMtPivot, 10, settings, item->descriptor, 1);
        g_mt.add_child(panel + MT_PANEL_CONTAINER, row);
        MtShowText(row + MT_ROW_TEXT, item->label);
        MtEntry* entry = &page->entries[position];
        entry->row = row;
        entry->item = item;
        MtApplyValue(entry, MtShownValue(item));
    }
    MtPut32(panel + family->count, (uint32_t)page->count);
    MtPutF(panel + family->x, 0);
    MtPutF(panel + family->gap, (float)gap);
    if (MtGet32(panel + spec->selected) >= (uint32_t)page->count) MtPut32(panel + spec->selected, 0);
    MtFinishPanel(panel, spec->family);
    view->hasPage = 1;
    return 1;
}

/* Intact while the panel lists exactly its rows and each still shows its item (a revert or a device change rebuilds the
 * panel natively; the DISPLAY panel's reload rewrites two rows' choice lists in place). */
static int MtPageIntact(const MtView* view)
{
    const MtPage* page = &view->page;
    const MtPanel* spec = &kMtPanels[page->index];
    uintptr_t panel = view->content + spec->offset;
    if (MtGet32(panel + kMtFamilies[spec->family].count) != (uint32_t)page->count) return 0;
    for (int i = 0; i < page->count; i++)
    {
        uintptr_t row = page->entries[i].row;
        const MtItem* item = page->entries[i].item;
        if (MtGet32(row + MT_ROW_SETTING) != MT_INERT_SETTING || !MtShowsText(row + MT_ROW_TEXT, item->label)) return 0;
        if (item->kind != HF_OPTION_SLIDER &&
            (MtGet32(row + MT_ROW_CHOICE_COUNT) != (uint32_t)item->choiceCount || MtGet32(row + MT_ROW_CHOICES) != item->labels[0]))
            return 0;
    }
    return 1;
}

static int MtSelectedEntry(const MtView* view)
{
    if (!view->hasPage) return -1;
    const MtPanel* spec = &kMtPanels[view->page.index];
    uint32_t index = MtGet32(view->content + spec->offset + spec->selected);
    return index < (uint32_t)view->page.count ? (int)index : -1;
}

/* ---- the MODS view ------------------------------------------------------------------------------------------------- */

/* Up to 8 mods: the list. More: page g_mtab.modsPage of 7 and the page control on the 8th button. */
static void MtButtonList(MtView* view)
{
    memset(view->buttons, 0, sizeof view->buttons);
    if (view->modCount <= MT_BUTTONS)
    {
        for (int i = 0; i < view->modCount; i++) view->buttons[i] = view->mods[i];
        return;
    }
    int first = g_mtab.modsPage * MT_PAGE_MODS;
    for (int i = 0; i < MT_PAGE_MODS; i++) view->buttons[i] = first + i < view->modCount ? view->mods[first + i] : NULL;
    view->buttons[MT_BUTTONS - 1] = view->control;
}

static int MtShows(const MtView* view, uint32_t index) { return index == 0 || (index < MT_BUTTONS && view->buttons[index] != NULL); }

static const char* MtPageText(const char* format, int page, int pages)
{
    char text[64];
    snprintf(text, sizeof text, format, page, pages);
    return MtIntern(text);
}

/* The page control: one choice row naming the pages; never an edit. */
static const MtCategory* MtPageControl(int count)
{
    int pages = (count + MT_PAGE_MODS - 1) / MT_PAGE_MODS;
    if (pages > (int)HF_OPTION_MAX_CHOICES) pages = (int)HF_OPTION_MAX_CHOICES;
    if (g_mtab.modsPage > pages - 1) g_mtab.modsPage = pages - 1;
    MtItem* item = &g_mtControl;
    memset(item, 0, sizeof *item);
    item->key = MT_NO_KEY;
    item->turn = 1;
    item->kind = HF_OPTION_CHOICE;
    item->label = MtIntern("Mods Page");
    item->description = MtIntern("The buttons show 7 mods at a time. Change the page to see the others.");
    item->value = g_mtab.modsPage;
    item->choiceCount = pages;
    for (int i = 0; i < pages; i++) item->choices[i] = MtPageText("%d / %d", i + 1, pages), item->labels[i] = MT_TEXT_TEMPLATE;
    item->descriptor = MtDescriptor(g_mtDescriptors[OPTIONS_MAX], item);
    MtCategory* c = &g_mtControlCategory;
    memset(c, 0, sizeof *c);
    c->title = MtPageText("PAGE %d OF %d", g_mtab.modsPage + 1, pages);
    c->modId = "";
    c->pages = pages;
    c->count = 1;
    c->items[0] = item;
    return c;
}

static void MtListMods(MtView* view)
{
    MtModelUpdate();
    view->revision = g_mtModelRevision;
    view->modCount = MtModelCategories(view->mods, MT_MAX_CATEGORIES);
    view->control = view->modCount > MT_BUTTONS ? MtPageControl(view->modCount) : NULL;
    MtButtonList(view);
}

static void MtTurnPage(MtView* view, int page)
{
    g_mtab.modsPage = page;
    g_mtControl.value = page;
    g_mtControlCategory.title = MtPageText("PAGE %d OF %d", page + 1, g_mtControlCategory.pages);
    MtButtonList(view);
    MtLabelCategories(view);
}

/* Switches the shown panel through the game's own selection (which deactivates the last panel and builds the next);
 * the shown one again goes through another panel to force a fresh build. */
static void MtSelectCategory(uintptr_t content, uint32_t shown, uint32_t target)
{
    if (shown == target)
    {
        uint32_t other = target == 0 ? 3u : 0u;
        MtPut32(content + MT_PREVIOUS_CATEGORY, shown);
        g_mt.select_category(content, (int)other);
        shown = other;
    }
    MtPut32(content + MT_PREVIOUS_CATEGORY, shown);
    g_mt.select_category(content, (int)target);
}

/* The idle gate, every frame: one read of the UI stack answers whether the escape menu is open; only then its screen.
 * 0 closed, 1 covered by another screen, 2 open on top (*screen set). */
static int MtEscapeMenu(uintptr_t game, uintptr_t* screen)
{
    uint32_t stack[MT_UI_ENTRIES + 1];
    uintptr_t ui, menu, at;
    if (!EvpPtr(game + MT_UI_STATE, &ui) || SafeRead(ui + MT_UI_STACK, stack, sizeof stack)) return 0;
    uint32_t depth = stack[MT_UI_ENTRIES];
    if (depth > MT_UI_ENTRIES) return 0;
    int open = 0;
    for (uint32_t i = 0; i < depth; i++) open |= stack[i] == MT_ESCAPE_TYPE;
    if (!open) return 0;
    if (stack[depth - 1] != MT_ESCAPE_TYPE) return 1;
    if (!EvpPtr(game + MT_MENU, &menu) || !EvpPtr(menu + MT_ESCAPE_SCREEN, &at) || at % 8 != 0) return 0;
    *screen = at;
    return 2;
}

static int MtActionTriggered(uintptr_t game, uint32_t group, uint32_t action)
{
    uintptr_t owner;
    if (!EvpPtr(game + MT_INPUT_OWNER, &owner) || group >= MT_ACTION_GROUPS || action >= MT_GROUP_ACTIONS) return 0;
    return MtGet8(owner + MT_ACTION_STATES + MT_ACTION_STRIDE * (MT_GROUP_ACTIONS * group + action)) != 0;
}

static int MtKeyTriggered(uintptr_t game, uintptr_t dialog, uint32_t key)
{
    return MtActionTriggered(game, MtGet32(dialog + key), MtGet32(dialog + key + 4));
}

/* The UNAPPLIED CHANGES dialog, decided as the game decides it: 0 none, 1 open, 2 confirmed, 3 cancelled. When the
 * game's input update ran first it already closed the dialog and cleared the confirm action (not the cancel action), so
 * a dialog seen open and closed since was confirmed unless its cancel action triggered. */
static int MtDialog(uintptr_t game, MtView* view)
{
    uintptr_t dialog = view->content + MT_DIALOG;
    uint32_t mode = MtGet32(dialog + MT_DIALOG_MODE);
    int open = MtGet32(dialog + MT_DIALOG_STATE) == MT_DIALOG_OPEN && (mode == MT_UNAPPLIED_MODE || mode == MT_NEUTRAL_MODE);
    int seen = view->dialog;
    if (!open)
    {
        view->dialog = MT_DIALOG_NONE;
        if (seen != MT_DIALOG_SEEN) return 0;
        return MtKeyTriggered(game, dialog, MT_DIALOG_CANCEL_ACTION) ? 3 : 2;
    }
    if (seen == MT_DIALOG_DECIDED) return 1;
    view->dialog = MT_DIALOG_DECIDED;
    if ((MtGet32(dialog + MT_DIALOG_CONFIRM) & MT_VISIBLE_FLAG) && MtKeyTriggered(game, dialog, MT_DIALOG_CONFIRM_ACTION)) return 2;
    if ((MtGet32(dialog + MT_DIALOG_CANCEL) & MT_VISIBLE_FLAG) && MtKeyTriggered(game, dialog, MT_DIALOG_CANCEL_ACTION)) return 3;
    view->dialog = MT_DIALOG_SEEN;
    return 1;
}

static void MtDiscardPending(MtView* view)
{
    MtDropPending();
    for (int i = 0; view->hasPage && i < view->page.count; i++)
    {
        MtEntry* entry = &view->page.entries[i];
        if (entry->item->key != MT_NO_KEY) MtApplyValue(entry, MtApplied(entry->item));
    }
    Note("mods tab: unapplied changes discarded");
}

/* Applies on the apply action, discards when the UNAPPLIED CHANGES dialog is confirmed, keeps the game's unapplied flag
 * in step. */
static void MtUpdateApply(uintptr_t game, MtView* view)
{
    if (g_mtab.pendingCount > 0)
    {
        int dialog = MtDialog(game, view);
        if (dialog == 2) MtDiscardPending(view);
        else if (dialog == 0 && MtActionTriggered(game, MT_MENU_GROUP, MT_APPLY_ACTION))
        {
            Note("mods tab: %d change(s) applied", MtApplyPending());
            g_mt.play_sound(0, MT_APPLY_SOUND);
        }
    }
    else view->dialog = MT_DIALOG_NONE;
    uint8_t unapplied = g_mtab.pendingCount > 0;
    if (unapplied != view->unapplied) { MtPut8(view->content + MT_UNAPPLIED, unapplied); view->unapplied = unapplied; }
}

/* The dialog switched to a mode the game's own confirm ignores once open (that confirm reloads the shown panel from the
 * live settings, which would rewrite the DISPLAY panel's rows). It finishes opening inside the visual pass, so this runs
 * after it. */
static void MtNeutralizeDialog(MtView* view)
{
    if (g_mtab.pendingCount == 0) return;
    uintptr_t dialog = view->content + MT_DIALOG;
    if (MtGet32(dialog + MT_DIALOG_STATE) != MT_DIALOG_OPEN) return;
    uint32_t mode = MtGet32(dialog + MT_DIALOG_MODE);
    if (mode == MT_UNAPPLIED_MODE) { MtPut32(dialog + MT_DIALOG_MODE, MT_NEUTRAL_MODE); mode = MT_NEUTRAL_MODE; }
    if (mode == MT_NEUTRAL_MODE && view->dialog == MT_DIALOG_NONE) view->dialog = MT_DIALOG_SEEN;
}

/* The screen runs the OPTIONS content's visual pass only while OPTIONS is the current tab, so MODS runs it itself; the
 * description then follows the selection. */
static void MtUpdateVisuals(MtView* view, float dt)
{
    g_mt.options_visual(view->content, dt);
    int entry = MtSelectedEntry(view);
    int64_t key = entry >= 0 ? (int64_t)view->page.generation * 64 + entry : MT_DESCRIBED_NONE;
    if (key != view->described) MtDescribe(view, entry);
}

static void MtEnterView(uintptr_t screen)
{
    uintptr_t content = screen + MT_OPTIONS_CONTENT;
    uint32_t shown = MtGet32(content + MT_CURRENT_CATEGORY);
    MtView* view = &g_mtab.view;
    memset(view, 0, sizeof *view);
    view->screen = screen;
    view->content = content;
    view->saved = shown < MT_BUTTONS ? shown : 0;
    view->described = MT_DESCRIBED_NEVER;
    view->values = g_optionRevision;
    MtValuesSeen();
    MtListMods(view);
    /* The tab switch hid every content: MODS shows the OPTIONS content, which the screen feeds input while its shown
     * content is OPTIONS. The column is laid out first with all nine buttons visible, so a button hidden here keeps its
     * place (else it stays at 0, 0 and draws over GAMEPLAY after GAME, MODS, OPTIONS). */
    MtPut32(screen + MT_SHOWN_CONTENT, MT_OPTIONS_TAB);
    g_mt.set_content_hidden(content, 0, 1);
    g_mt.options_visual(content, 0);
    MtLabelCategories(view);
    if (shown != 0) MtSelectCategory(content, shown, 0);
    g_mtab.hasView = 1;
    (void)MtBuildPage(view, 0);
    Note("mods tab: opened, %d mod(s) with options", view->modCount);
}

static void MtLeaveView(MtView* view)
{
    uintptr_t content = view->content;
    uint32_t shown = view->hasPage ? (uint32_t)view->page.index : MtGet32(content + MT_CURRENT_CATEGORY);
    view->hasPage = 0;
    MtDropPending(); /* the game lets the view go only once the edits are applied or discarded */
    MtPut8(content + MT_UNAPPLIED, 0);
    MtReleaseDescription(content);
    MtRestoreCategories(content);
    if (shown >= MT_BUTTONS) shown = MT_CATEGORIES; /* no panel to deactivate */
    MtSelectCategory(content, shown, view->saved);
    g_mtab.hasView = 0;
}

static void MtRefreshView(MtView* view)
{
    MtListMods(view);
    MtLabelCategories(view);
    uint32_t current = view->hasPage ? (uint32_t)view->page.index : 0;
    view->hasPage = 0;
    if (!MtShows(view, current)) { MtSelectCategory(view->content, current, 0); current = 0; }
    (void)MtBuildPage(view, current);
}

/* Values set from code since the view last looked: shown on their rows. */
static void MtShowSetValues(MtView* view)
{
    static uint8_t changed[OPTIONS_MAX];
    view->values = g_optionRevision;
    if (MtValuesSet(changed) == 0) return;
    for (int i = 0; view->hasPage && i < view->page.count; i++)
    {
        MtEntry* entry = &view->page.entries[i];
        if (entry->item->key != MT_NO_KEY && changed[entry->item->key]) MtApplyValue(entry, MtApplied(entry->item));
    }
}

/* The player's row changes: the page control turns the pages, an option's row becomes a pending edit. A value the item
 * cannot take is set back once; a row left at NaN after that is left alone. */
static void MtPollRows(MtView* view)
{
    for (int i = 0; i < view->page.count && view->hasPage; i++)
    {
        MtEntry* entry = &view->page.entries[i];
        double raw = MtRowValue(entry), value;
        if (raw == entry->raw) continue;
        if (!MtDecode(entry->item, raw, &value))
        {
            if (raw == raw || entry->raw == entry->raw) MtApplyValue(entry, MtShownValue(entry->item));
            continue;
        }
        entry->raw = raw;
        MtShowChoice(entry);
        if (entry->item->turn) MtTurnPage(view, (int)value);
        else MtSetPending(entry->item, value);
    }
}

/* The page as the game left it (a panel switch, a rebuild), then values set from code, then the player's changes. */
static void MtMaintainView(MtView* view)
{
    uint32_t current = MtGet32(view->content + MT_CURRENT_CATEGORY);
    if (view->revision != (int)g_optionCount) MtRefreshView(view);
    else if (view->hasPage && current == (uint32_t)view->page.index)
    {
        if (!MtPageIntact(view)) (void)MtBuildPage(view, current);
    }
    else if (MtShows(view, current)) (void)MtBuildPage(view, current); /* the game already switched panels */
    else
    {
        MtSelectCategory(view->content, current, 0);
        (void)MtBuildPage(view, 0);
    }
    if (view->values != g_optionRevision) MtShowSetValues(view);
    MtPollRows(view);
}

/* ---- the tick ------------------------------------------------------------------------------------------------------ */

static void MtDropView(void)
{
    g_mtab.hasView = 0;
    MtDropPending();
}

/* Another mod's MODS tab is on the bar: this one leaves it alone for the session. */
static void MtStandDown(void)
{
    if (g_mtab.hasView) MtLeaveView(&g_mtab.view);
    g_mtab.state = -1;
    Note("mods tab: off: another mod's MODS tab is on the escape menu (the Vanilla Plus Megapack's?); HellForge's options go there through the bundled Lua mod");
}

static void MtOpen(uintptr_t game, uintptr_t screen, float dt)
{
    if (g_mtab.hasView && g_mtab.view.screen != screen) MtDropView();
    if (MtForeignTab(screen + MT_TAB_BAR)) { MtStandDown(); return; }
    if (!MtEnsureTab(screen)) { if (g_mtab.hasView) MtDropView(); return; }
    uint32_t tab = MtGet32(screen + MT_TAB_BAR + MT_TAB_CURRENT);
    if (g_mtab.hasView)
    {
        if (tab != MT_MODS_TAB) MtLeaveView(&g_mtab.view);
        else MtMaintainView(&g_mtab.view);
    }
    else if (tab == MT_MODS_TAB) MtEnterView(screen);
    if (!g_mtab.hasView) return;
    MtUpdateApply(game, &g_mtab.view);
    MtUpdateVisuals(&g_mtab.view, dt);
    MtNeutralizeDialog(&g_mtab.view);
}

/* game.dll's functions read as reviewed (through hooks), then its labels and setting descriptor, which the game can fill
 * in a little after its code is decoded: a look every 30 frames for at most 60 s. 1 ready, 0 not yet, -1 never. */
static int MtVerify(uintptr_t game, const char** why)
{
    for (size_t i = 0; i < MT_PINS; i++)
        if (!ApiCodeMatches(game + kMtPins[i].rva, kMtPins[i].bytes)) { *why = "the menu's code is not the reviewed one"; return 0; }
    for (uint32_t i = 0; i < 3; i++)
        if (EvpU32(game + MT_TAB_LABELS_RVA + i * 4u, 0) != kMtTabLabels[i]) { *why = "the escape menu's tab labels are not the reviewed ones"; return 0; }
    for (uint32_t i = 0; i < 9; i++)
        if (EvpU32(game + MT_CATEGORY_TABLE_RVA + i * MT_CATEGORY_RECORD + 8u, 0) != kMtCategoryLabels[i]) { *why = "the OPTIONS categories are not the reviewed ones"; return 0; }
    if (EvpU32(game + MT_INERT_DESCRIPTOR_RVA, 0) != MT_INERT_SETTING || EvpU32(game + MT_INERT_DESCRIPTOR_RVA + 4u, 1) != MT_INERT_CATEGORY ||
        EvpU32(game + MT_INERT_DESCRIPTOR_RVA + 12u, 1) != 0)
    { *why = "the setting descriptors are not the reviewed ones"; return 0; }
    for (size_t i = 0; i < MT_PINS; i++) memcpy((uint8_t*)&g_mt + kMtPins[i].field, &(uintptr_t){game + kMtPins[i].rva}, sizeof(uintptr_t));
    return 1;
}

/* Waits for the game's code and data as reviewed: 1 ready, 0 not yet (or off for the session). */
static int MtReady(uintptr_t game, float dt)
{
    const char* why = "unsupported game build";
    g_mtab.waited += dt;
    if (game == 0 || strcmp(g_build, "25480438")) { g_mtab.state = -1; Note("mods tab: off: %s", why); return 0; }
    if (g_mtab.frames++ % 30 != 0) return 0;
    if (!MtVerify(game, &why))
    {
        if (g_mtab.waited >= 60.0f) { g_mtab.state = -1; Note("mods tab: off: %s", why); }
        return 0;
    }
    g_mtab.state = 1;
    Note("mods tab: on (%.1f s after the game loaded), %ld option(s) so far", g_mtab.waited, (long)g_optionCount);
    return 1;
}

static void MtTick(HfMod* mod, float dt, void* user)
{
    (void)mod, (void)user;
    if (g_mtab.state < 0 || !g_fired[HF_EVENT_GAME_LOADED]) return;
    uintptr_t game = ApiModuleBase("game.dll", NULL), screen = 0;
    if (g_mtab.state == 0 && !MtReady(game, dt)) return;
    int status = MtEscapeMenu(game, &screen);
    if (status == 0 && g_mtab.hasView) MtDropView();
    if (status == 2) MtOpen(game, screen, dt);
}

/* The Megapack draws OPTIONS > MODS itself while its Mod Options Menu component is on: settings.ini's [components]
 * mod_options_menu = off (in the %LOCALAPPDATA% HellForge gives the game) switches it off. */
static int MtMegapackMenu(void)
{
    int loaded = 0;
    for (int i = 0; i < g_modCount; i++)
        if (!strcmp(g_mods[i].id, "cowboybingus.vanilla-plus-megapack") && !g_mods[i].removed) loaded = 1;
    if (!loaded) return 0;
    wchar_t path[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 64) return 1;
    wcscat(path, L"\\CowboyBingus\\VanillaPlusMegapack\\settings.ini");
    FILE* f = _wfopen(path, L"r");
    if (f == NULL) return 1; /* every component starts on */
    char line[256];
    int components = 0, on = 1;
    while (fgets(line, sizeof line, f))
    {
        char key[64] = {0}, value[32] = {0};
        if (line[0] == '[') { components = !strncmp(line, "[components]", 12); continue; }
        if (!components || sscanf(line, " %63[^= \t] = %31s", key, value) != 2 || strcmp(key, "mod_options_menu")) continue;
        on = !(!_stricmp(value, "off") || !_stricmp(value, "false") || !strcmp(value, "0"));
    }
    fclose(f);
    return on;
}

/* At the loader's start (after the mods set up): the tick, unless the Megapack's menu is the one on the escape menu. */
static void ModsTabStart(void)
{
    g_mtab.title = MtIntern("MODS");
    g_mtab.empty = MtIntern("NO MOD OPTIONS INSTALLED");
    if (MtMegapackMenu())
    {
        g_mtab.state = -1;
        Note("mods tab: off: the Vanilla Plus Megapack's Mod Options Menu draws OPTIONS > MODS; HellForge's options go there through the bundled Lua mod");
        return;
    }
    g_mtMod.api = &g_api;
    snprintf(g_mtMod.id, sizeof g_mtMod.id, "hellforge.mods-tab");
    g_mtMod.log = g_log;
    g_mtMod.loaded = 1;
    if (!ApiOnGameThread(&g_mtMod, MtTick, NULL)) { g_mtab.state = -1; Note("mods tab: off: no game-thread tick"); }
}
