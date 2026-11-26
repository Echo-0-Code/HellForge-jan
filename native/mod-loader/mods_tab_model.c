/* The MODS tab's texts, the categories and rows it lists over the SDK 30 options, and their values: edits pending until
 * APPLY, and values a mod sets from code. Included by mods_tab.c after its game constants. */

/* ---- texts ---------------------------------------------------------------------------------------------------------
 * Text widgets keep a pointer to the text they show, and a hidden widget may point at one until the game sets it up
 * again, so every text lives in one buffer for the session, one buffer per distinct text: a widget shows a text when its
 * argument is that buffer. */

#define MT_POOL (256u * 1024u)
#define MT_TEXTS 2048
static char g_mtPool[MT_POOL];
static uint32_t g_mtUsed, g_mtAt[MT_TEXTS], g_mtHash[MT_TEXTS];
static int g_mtTexts;

static const char* MtIntern(const char* text)
{
    uint32_t h = 2166136261u;
    for (const unsigned char* p = (const unsigned char*)text; *p; p++) h = (h ^ *p) * 16777619u;
    for (int i = 0; i < g_mtTexts; i++)
        if (g_mtHash[i] == h && !strcmp(g_mtPool + g_mtAt[i], text)) return g_mtPool + g_mtAt[i];
    size_t n = strlen(text) + 1;
    if (g_mtTexts >= MT_TEXTS || n > MT_POOL - g_mtUsed) return "?";
    memcpy(g_mtPool + g_mtUsed, text, n);
    g_mtAt[g_mtTexts] = g_mtUsed;
    g_mtHash[g_mtTexts++] = h;
    g_mtUsed += (uint32_t)n;
    return g_mtPool + g_mtAt[g_mtTexts - 1];
}

/* A UTF-8 sequence at p: its length and code point, or 0 when it is not valid UTF-8. */
static int MtUtf8(const unsigned char* p, uint32_t* out)
{
    if (p[0] < 0x80) { *out = p[0]; return 1; }
    int n = (p[0] & 0xE0) == 0xC0 ? 2 : (p[0] & 0xF0) == 0xE0 ? 3 : (p[0] & 0xF8) == 0xF0 ? 4 : 0;
    if (n == 0) return 0;
    uint32_t v = p[0] & (0x7F >> n);
    for (int i = 1; i < n; i++)
    {
        if ((p[i] & 0xC0) != 0x80) return 0;
        v = v << 6 | (p[i] & 0x3F);
    }
    if ((n == 2 && v < 0x80) || (n == 3 && v < 0x800) || (n == 4 && (v < 0x10000 || v > 0x10FFFF)) || (v >= 0xD800 && v <= 0xDFFF)) return 0;
    *out = v;
    return n;
}

/* The text as shown (trimmed), interned; NULL when blank or not valid UTF-8 (add_option refused control characters). */
static const char* MtShown(const char* text)
{
    char work[1664];
    if (text == NULL) return NULL;
    while (*text == ' ') text++;
    size_t n = strlen(text);
    if (n >= sizeof work) return NULL;
    while (n > 0 && text[n - 1] == ' ') n--;
    if (n == 0) return NULL;
    memcpy(work, text, n);
    work[n] = 0;
    uint32_t v;
    for (const unsigned char* p = (const unsigned char*)work; *p;)
    {
        int k = MtUtf8(p, &v);
        if (k == 0) return NULL;
        p += k;
    }
    return MtIntern(work);
}

/* Upper case in Latin-1, Latin Extended-A, Greek and Cyrillic (the game's font), interned. */
/* Latin Extended-A pairs upper and lower case, upper first: odd-upper in 0x139-0x148 and 0x179-0x17E, even elsewhere. */
static uint32_t MtUpperExtendedA(uint32_t v)
{
    if (v == 0x130 || v == 0x131 || v == 0x138 || v == 0x149 || v == 0x17F) return v;
    int oddUpper = (v >= 0x139 && v <= 0x148) || (v >= 0x179 && v <= 0x17E);
    return v % 2 == (oddUpper ? 0u : 1u) ? v - 1 : v;
}

static uint32_t MtUpperChar(uint32_t v)
{
    if (v < 0x80) return v >= 'a' && v <= 'z' ? v - 32 : v;
    if (v >= 0xE0 && v <= 0xFE && v != 0xF7) return v - 32;
    if (v == 0xFF) return 0x178;
    if (v >= 0x100 && v <= 0x17F) return MtUpperExtendedA(v);
    if ((v >= 0x3B1 && v <= 0x3C9 && v != 0x3C2) || (v >= 0x430 && v <= 0x44F)) return v - 32;
    return v >= 0x450 && v <= 0x45F ? v - 80 : v;
}

static int MtEncode(uint32_t v, char* out)
{
    if (v < 0x80) { out[0] = (char)v; return 1; }
    if (v < 0x800) { out[0] = (char)(0xC0 | v >> 6); out[1] = (char)(0x80 | (v & 0x3F)); return 2; }
    out[0] = (char)(0xE0 | v >> 12), out[1] = (char)(0x80 | (v >> 6 & 0x3F)), out[2] = (char)(0x80 | (v & 0x3F));
    return 3;
}

static const char* MtUpper(const char* text)
{
    char work[1664];
    size_t at = 0;
    for (const unsigned char* p = (const unsigned char*)text; *p && at + 4 < sizeof work;)
    {
        uint32_t v;
        int k = MtUtf8(p, &v);
        if (k == 0) { work[at++] = (char)*p++; continue; }
        uint32_t u = MtUpperChar(v);
        if (u == v) memcpy(work + at, p, (size_t)k), at += (size_t)k;
        else at += (size_t)MtEncode(u, work + at);
        p += k;
    }
    work[at] = 0;
    return MtIntern(work);
}

/* ---- the model: one category per mod with options ------------------------------------------------------------------ */

#define MT_NO_KEY (-1)
#define MT_MAX_CATEGORIES (MT_PAGE_MODS * (int)HF_OPTION_MAX_CHOICES)
#define MT_DESCRIPTOR_SIZE 216u
#define MT_DESCRIPTOR_STRIDE 224u
#define MT_OFF_TEXT 0xA090BE2Eu
#define MT_ON_TEXT 0x13DC1DA2u

typedef struct MtItem
{
    int key;                    /* the option's index (g_options), or MT_NO_KEY: the page control */
    int turn;                   /* the page control: turning it turns the pages */
    double value;               /* the page control's: the page shown, from 0 */
    uint32_t kind;              /* HF_OPTION_... */
    const char* label;
    const char* description;    /* or NULL */
    int choiceCount;
    const char* choices[HF_OPTION_MAX_CHOICES];
    uint32_t labels[HF_OPTION_MAX_CHOICES]; /* the game's own word, or MT_TEXT_TEMPLATE */
    double min, max, step;
    int decimals, gap;
    const uint8_t* descriptor;  /* 216 bytes, 16-byte aligned, as the game's setting descriptors */
} MtItem;

typedef struct MtCategory
{
    const char* title;
    const char* modId;
    int sequence, count, pages;
    const MtItem* items[MT_MAX_ROWS];
} MtCategory;

/* Choice words the game translates itself; any other text shows as it is. */
typedef struct MtWord { const char* word; uint32_t label; } MtWord;
static const MtWord kMtWords[] = {
    {"OFF", 0xA090BE2Eu}, {"ON", 0x13DC1DA2u}, {"NO", 0xEF21C0C2u}, {"YES", 0x30DBDB29u},
    {"LOW", 0xE1F9AB36u}, {"MEDIUM", 0x3134D1FEu}, {"HIGH", 0x208F9597u}, {"ULTRA", 0x99FFB498u},
    {"DEFAULT", 0x38E6B4E4u}, {"CUSTOM", 0x2ACF00B1u}, {"NORMAL", 0xF3222616u}, {"INVERTED", 0x05478062u},
    {"WEAK", 0xE7B8332Eu}, {"STRONG", 0xD9709682u}, {"BASIC", 0xAAB9E5DAu}, {"ADVANCED", 0xE0B81607u},
    {"FULL", 0x3BA052B7u}, {"PERFORMANCE", 0x5097DF8Cu}, {"BALANCED", 0xE2135504u}, {"QUALITY", 0x61F69557u},
    {"GLOBAL", 0x1D256A7Bu}, {"ALWAYS", 0xECCB0C50u}, {"DISABLED", 0xE480CC2Bu}, {"HIDDEN", 0xAAE829EDu},
    {"VISIBLE", 0xBCAB6809u}, {"SMALL", 0x818D7022u}, {"LARGE", 0xF8C59D6Au}, {"SHORT", 0xB008F033u},
    {"DYNAMIC", 0xF54D4908u}, {"HOLD", 0x639EE7C1u}, {"PRESS", 0x91B91DD3u}, {"TAP", 0x26603652u},
};

static MtItem g_mtItems[OPTIONS_MAX];
static MtCategory g_mtCategories[MT_MAX_CATEGORIES];
static int g_mtCategoryCount, g_mtModelRevision = -1;
static uint8_t g_mtRefused[OPTIONS_MAX];
static MtItem g_mtControl;
static MtCategory g_mtControlCategory;
static _Alignas(16) uint8_t g_mtDescriptors[OPTIONS_MAX + 1][MT_DESCRIPTOR_STRIDE];

static void MtPutWord(uint8_t* blob, int index, uint32_t value) { memcpy(blob + index * 4, &value, 4); }
static void MtPutFloat(uint8_t* blob, int index, float value) { memcpy(blob + index * 4, &value, 4); }

/* The game's descriptor layout (a float slider of 0.01 steps there uses +48 = 6, +52 = 1, format 0x6220). */
static const uint8_t* MtDescriptor(uint8_t* blob, const MtItem* item)
{
    memset(blob, 0, MT_DESCRIPTOR_SIZE);
    MtPutWord(blob, 0, MT_INERT_SETTING);
    MtPutWord(blob, 1, MT_INERT_CATEGORY);
    MtPutWord(blob, 4, MT_TEXT_TEMPLATE);
    if (item->kind == HF_OPTION_SLIDER)
    {
        MtPutWord(blob, 2, item->decimals == 0 ? 0u : 1u); /* an integer or a float slider */
        MtPutFloat(blob, 9, (float)item->min);
        MtPutFloat(blob, 10, (float)item->max);
        MtPutFloat(blob, 11, (float)item->step);
        MtPutFloat(blob, 12, item->decimals == 0 ? 20.0f : 6.0f);
        MtPutFloat(blob, 13, 1.0f);
        MtPutWord(blob, 14, item->decimals == 0 ? 0u : 0x6200u + (uint32_t)item->decimals * 16u);
        return blob;
    }
    MtPutWord(blob, 2, 2u); /* a selector */
    MtPutWord(blob, 16, (uint32_t)item->choiceCount);
    for (int i = 0; i < item->choiceCount; i++) MtPutWord(blob, 17 + i, item->labels[i]);
    return blob;
}

static uint32_t MtWordLabel(const char* text)
{
    for (size_t i = 0; i < sizeof kMtWords / sizeof kMtWords[0]; i++)
        if (!strcmp(kMtWords[i].word, text)) return kMtWords[i].label;
    return MT_TEXT_TEMPLATE;
}

/* Decimal places a slider shows: as many as its step needs, at most 3, at least 1 when min is not whole. */
static int MtDecimals(double min, double step)
{
    int d = 0;
    while (d < 3 && fabs(step * pow(10, d) - floor(step * pow(10, d) + 0.5)) > 1e-6) d++;
    if (fmod(min, 1) != 0 && d < 1) d = 1;
    return d;
}

/* The item for option `index` (read under the options lock), or why it cannot be shown. */
static const char* MtFields(MtItem* item, const ModOption* o, int index)
{
    memset(item, 0, sizeof *item);
    item->key = index;
    item->kind = o->kind;
    item->gap = o->gap != 0;
    if ((item->label = MtShown(o->label)) == NULL) return "its label is not text the menu can show";
    if (o->description[0] && (item->description = MtShown(o->description)) == NULL) return "its description is not text the menu can show";
    if (o->kind == HF_OPTION_TOGGLE)
    {
        item->choiceCount = 2;
        item->labels[0] = MT_OFF_TEXT, item->labels[1] = MT_ON_TEXT;
    }
    else if (o->kind == HF_OPTION_CHOICE)
    {
        item->choiceCount = (int)o->choiceCount;
        for (int i = 0; i < item->choiceCount; i++)
        {
            const char* text = MtShown(o->choiceText[i]);
            if (text == NULL) return "a choice is not text the menu can show";
            item->choices[i] = MtUpper(text);
            item->labels[i] = MtWordLabel(item->choices[i]);
        }
    }
    else
    {
        item->min = o->min, item->max = o->max, item->step = o->step;
        item->decimals = MtDecimals(o->min, o->step);
    }
    item->descriptor = MtDescriptor(g_mtDescriptors[index], item);
    return NULL;
}

static MtCategory* MtModCategory(const ModOption* o)
{
    for (int i = 0; i < g_mtCategoryCount; i++)
        if (!strcmp(g_mtCategories[i].modId, o->modId)) return &g_mtCategories[i];
    if (g_mtCategoryCount >= MT_MAX_CATEGORIES) return NULL;
    MtCategory* c = &g_mtCategories[g_mtCategoryCount];
    memset(c, 0, sizeof *c);
    const char* name = MtShown(o->modName);
    c->title = MtUpper(name != NULL ? name : o->modId);
    c->modId = MtIntern(o->modId);
    c->sequence = ++g_mtCategoryCount;
    return c;
}

/* Option i's row in its mod's category: NULL, or why it is left out. Under the options lock. */
static const char* MtAddItem(int i)
{
    const ModOption* o = &g_options[i];
    if (o->flags & HF_OPTION_EDITOR_ONLY) return NULL; /* the editor's MOD VALUES alone shows it */
    MtItem* item = &g_mtItems[i];
    const char* why = MtFields(item, o, i);
    if (why != NULL) return why;
    MtCategory* category = MtModCategory(o);
    if (category == NULL) return "the menu has no room for another mod";
    if (category->count >= MT_MAX_ROWS) return "its mod already shows 32 options";
    category->items[category->count++] = item;
    return NULL;
}

/* The model for the options added so far: rebuilt when their count changed (values are read live). */
static void MtModelUpdate(void)
{
    int count = (int)g_optionCount;
    if (count == g_mtModelRevision) return;
    g_mtModelRevision = count;
    g_mtCategoryCount = 0;
    AcquireSRWLockShared(&g_optionsLock);
    for (int i = 0; i < count && i < OPTIONS_MAX; i++)
    {
        const char* why = MtAddItem(i);
        if (why != NULL && !g_mtRefused[i]) Note("mods tab: %s/%s is not shown: %s", g_options[i].modId, g_options[i].id, why);
        g_mtRefused[i] |= why != NULL;
    }
    ReleaseSRWLockShared(&g_optionsLock);
}

/* Buttons in alphabetical order, the same title in the order the mods added their first option. */
static int MtModelCategories(const MtCategory** out, int capacity)
{
    int n = 0;
    for (int i = 0; i < g_mtCategoryCount && n < capacity; i++)
    {
        const MtCategory* c = &g_mtCategories[i];
        if (c->count == 0) continue;
        int at = n++;
        while (at > 0)
        {
            int order = strcmp(c->title, out[at - 1]->title);
            if (order > 0 || (order == 0 && c->sequence > out[at - 1]->sequence)) break;
            out[at] = out[at - 1];
            at--;
        }
        out[at] = c;
    }
    return n;
}

/* ---- the view's state --------------------------------------------------------------------------------------------- */

#define MT_DESCRIBED_NEVER (-2)
#define MT_DESCRIBED_NONE (-1)
#define MT_DIALOG_NONE 0
#define MT_DIALOG_SEEN 1
#define MT_DIALOG_DECIDED 2

typedef struct MtEntry { uintptr_t row; const MtItem* item; double raw; } MtEntry;
typedef struct MtPage { int index, count, generation; MtEntry entries[MT_MAX_ROWS]; } MtPage;
typedef struct MtView
{
    uintptr_t screen, content;
    int revision;               /* the model's when listed */
    LONG values;                /* g_optionRevision when the rows last showed every value set from code */
    uint32_t saved;             /* the category OPTIONS showed, given back on leaving */
    int64_t described;
    uint8_t unapplied;
    int dialog, modCount;
    const MtCategory* mods[MT_MAX_CATEGORIES];
    const MtCategory* control;
    const MtCategory* buttons[MT_BUTTONS];
    int hasPage;
    MtPage page;
} MtView;

static struct
{
    int state;                  /* 0 waiting for the game, 1 on, -1 off for the session (modloader.log says why) */
    float waited;
    int frames, hasView, modsPage, generation;
    const char* title;
    const char* empty;
    MtView view;
    int pendingCount;
    uint8_t pendingSet[OPTIONS_MAX];
    double pending[OPTIONS_MAX];
    uint32_t seen[OPTIONS_MAX]; /* each option's serial as the open view shows it */
    int seenCount;
} g_mtab;

static HfMod g_mtMod;

/* ---- values -------------------------------------------------------------------------------------------------------- */

static double MtApplied(const MtItem* item)
{
    if (item->key == MT_NO_KEY) return item->value;
    AcquireSRWLockShared(&g_optionsLock);
    double v = g_options[item->key].value;
    ReleaseSRWLockShared(&g_optionsLock);
    return v;
}

static double MtShownValue(const MtItem* item)
{
    if (item->key == MT_NO_KEY) return item->value;
    return g_mtab.pendingSet[item->key] ? g_mtab.pending[item->key] : MtApplied(item);
}

/* A slider's value snapped to its step and shown decimals: 0 for NaN and the infinities. */
static int MtSnap(const MtItem* item, double value, double* out)
{
    if (!(value > -HUGE_VAL && value < HUGE_VAL)) return 0;
    double steps = floor((value - item->min) / item->step + 0.5);
    double snapped = fmin(item->max, fmax(item->min, item->min + steps * item->step));
    char text[64];
    snprintf(text, sizeof text, "%.*f", item->decimals, snapped);
    *out = strtod(text, NULL);
    return 1;
}

/* An edit waits for APPLY; editing back to the applied value withdraws it. */
static void MtSetPending(const MtItem* item, double value)
{
    if (item->key == MT_NO_KEY) return;
    int had = g_mtab.pendingSet[item->key], has = value != MtApplied(item);
    g_mtab.pendingSet[item->key] = (uint8_t)has;
    g_mtab.pending[item->key] = has ? value : 0;
    if (had != has) g_mtab.pendingCount += has ? 1 : -1;
}

static void MtDropPending(void)
{
    if (g_mtab.pendingCount == 0) return;
    memset(g_mtab.pendingSet, 0, sizeof g_mtab.pendingSet);
    g_mtab.pendingCount = 0;
}

static uint32_t MtSerial(int index)
{
    AcquireSRWLockShared(&g_optionsLock);
    uint32_t serial = g_options[index].serial;
    ReleaseSRWLockShared(&g_optionsLock);
    return serial;
}

static void MtValuesSeen(void)
{
    g_mtab.seenCount = (int)g_optionCount;
    for (int i = 0; i < g_mtab.seenCount; i++) g_mtab.seen[i] = MtSerial(i);
}

/* Options whose value changed since the view last looked (set_option, or a mod's on_change): marked in changed[], their
 * pending edits dropped; how many. */
static int MtValuesSet(uint8_t* changed)
{
    int count = (int)g_optionCount, n = 0;
    for (int i = 0; i < count; i++)
    {
        uint32_t serial = MtSerial(i);
        changed[i] = (uint8_t)(i < g_mtab.seenCount && serial != g_mtab.seen[i]);
        g_mtab.seen[i] = serial;
        if (!changed[i]) continue;
        if (g_mtab.pendingSet[i]) g_mtab.pendingSet[i] = 0, g_mtab.pendingCount--;
        n++;
    }
    g_mtab.seenCount = count;
    return n;
}

/* Every pending edit applied in the order the options were added: saved, and each mod's on_change called. */
static int MtApplyPending(void)
{
    int applied = 0;
    for (int key = 0; key < OPTIONS_MAX && g_mtab.pendingCount > 0; key++)
    {
        if (!g_mtab.pendingSet[key]) continue;
        g_mtab.pendingSet[key] = 0;
        g_mtab.pendingCount--;
        applied++;
        if (!hellforge_lua_option_applied((uint32_t)key, g_mtab.pending[key])) Note("mods tab: a change was refused: %g", g_mtab.pending[key]);
        if (key < g_mtab.seenCount) g_mtab.seen[key] = MtSerial(key); /* this view's own change, already on its row */
    }
    return applied;
}
