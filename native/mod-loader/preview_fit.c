/* SDK 46, set_preview_fit (build 25480438; docs/modding/ui.md). Included after loadout_preview.c.
 * The Armory preview's update (0x18EA830) fits its camera to a box (+0x890 size, +0x87C centre; +0x88C/+0x83C the box
 * it blends from) only when the item's kind changes, so every support card kept the first one's fit. A new item (+0x7E4,
 * its loadout id) zeroes the size for a few updates, which makes the game fit that item's own box; fits then scale it. */
#define PREVIEW_FIT_UPDATE 0x18EA830u
#define PREVIEW_FIT_ITEM 0x7E4u
#define PREVIEW_FIT_UNIT 0x7F8u /* the item's row: +8 its unit resource */
#define PREVIEW_FIT_DELAY 0x89Cu
#define PREVIEW_FIT_PREV_CENTRE 0x83Cu
#define PREVIEW_FIT_CENTRE 0x87Cu
#define PREVIEW_FIT_PREV_SIZE 0x88Cu
#define PREVIEW_FIT_SIZE 0x890u
#define PREVIEW_FIT_REFITS 20 /* updates after a pick: the new unit spawns and streams in meanwhile */
#define PREVIEW_FIT_MAX 128u
#define PREVIEW_FIT_LOGS 64

typedef void (*PreviewFitUpdateFn)(uint8_t* preview, uint32_t unit, float dt);
typedef struct PreviewFitRow { uint32_t item; float scale; float offset[3]; } PreviewFitRow;
static PreviewFitRow g_previewFits[PREVIEW_FIT_MAX];
static uint32_t g_previewFitCount;
static SRWLOCK g_previewFitLock = SRWLOCK_INIT;
static PreviewFitUpdateFn g_previewFitOriginal;
static int g_previewFitState; /* 0 not yet, 1 hooked, -1 off */
static uint8_t* g_previewFitObject;
static uint32_t g_previewFitItem;
static int g_previewFitRefits;
static int g_previewFitLogs;

static const uint8_t kPreviewFitEntry[] = {
    0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x78,0x20,0x55,0x48,0x8d,0xa8,0xe8,0xfe,0xff,0xff,0x48,0x81,0xec,0x10,0x02};
/* 0x18EAFF9..0x18EB053: the blend reads the centres (+0x87C/+0x880/+0x884, +0x840/+0x844) and sizes (+0x88C, +0x890). */
static const uint8_t kPreviewFitBlend[] = {
    0xf3,0x0f,0x59,0x97,0x7c,0x08,0x00,0x00,0xf3,0x0f,0x58,0xd0,0x41,0x0f,0x28,0xc6,0xf3,0x0f,0x59,0x87,0x40,0x08,0x00,0x00,
    0xf3,0x0f,0x11,0x55,0x90,0x0f,0x28,0xd1,0xf3,0x0f,0x59,0x97,0x80,0x08,0x00,0x00,0xf3,0x0f,0x58,0xd0,0x41,0x0f,0x28,0xc6,
    0xf3,0x0f,0x59,0x87,0x44,0x08,0x00,0x00,0xf3,0x44,0x0f,0x59,0xb7,0x8c,0x08,0x00,0x00,0xf3,0x0f,0x11,0x54,0x24,0x40,0x0f,
    0x28,0xd1,0xf3,0x0f,0x59,0x97,0x84,0x08,0x00,0x00,0xf3,0x0f,0x59,0x8f,0x90,0x08,0x00,0x00};
/* 0x18EAD8A: a size of 0 refits (the box query for the slot +0x7F0 and kind +0x7DC, as on a kind change). */
static const uint8_t kPreviewFitZero[] = {
    0xf3,0x0f,0x10,0x87,0x90,0x08,0x00,0x00,0x41,0x0f,0x2e,0xc5,0x0f,0x8a,0x9b,0x00,0x00,0x00,0x0f,0x85,0x95,0x00,0x00,0x00,
    0x44,0x8b,0x87,0xf0,0x07,0x00,0x00,0x48,0x8d,0x4d,0xe0,0x33,0xd2,0x48,0x89,0x74,0x24,0x20,0xe8,0x97,0xef,0xff,0xff};
/* 0x18EBEAF in the preview's pick (0x18EBEA0): cmp/mov [rcx+0x7E4], edx (the picked item's loadout id). */
static const uint8_t kPreviewFitItem[] = {0x39,0x91,0xe4,0x07,0x00,0x00,0x0f,0x84,0x7e,0x00,0x00,0x00,0x89,0x91,0xe4,0x07,0x00,0x00};

static int PreviewFitFind(uint32_t item, PreviewFitRow* out)
{
    int found = 0;
    AcquireSRWLockShared(&g_previewFitLock);
    for (uint32_t i = 0; i < g_previewFitCount; i++)
        if (g_previewFits[i].item == item) { *out = g_previewFits[i]; found = 1; break; }
    ReleaseSRWLockShared(&g_previewFitLock);
    return found;
}

static uint64_t PreviewFitUnit(const uint8_t* preview)
{
    uint64_t row = 0, unit = 0;
    if (!SafeRead((uintptr_t)(preview + PREVIEW_FIT_UNIT), &row, sizeof row) && row) SafeRead((uintptr_t)row + 8, &unit, sizeof unit);
    return unit;
}

static void PreviewFitScale(uint8_t* preview, uint32_t item)
{
    float* size = (float*)(preview + PREVIEW_FIT_SIZE);
    float* centre = (float*)(preview + PREVIEW_FIT_CENTRE);
    PreviewFitRow fit;
    const int has = PreviewFitFind(item, &fit);
    if (g_previewFitRefits == 0 && g_previewFitLogs < PREVIEW_FIT_LOGS)
    {
        g_previewFitLogs++;
        Note("preview fit: item %08x unit %016llx box size %.3f m, centre (%.3f, %.3f, %.3f)%s", item,
            (unsigned long long)PreviewFitUnit(preview), (double)*size, (double)centre[0], (double)centre[1], (double)centre[2],
            has ? ", scaled by its set_preview_fit" : "");
    }
    if (!has) return;
    /* The camera's distance follows the size, and the item moves by minus the centre. No blend: the fit is the item's. */
    *size /= fit.scale;
    for (int k = 0; k < 3; k++) centre[k] -= fit.offset[k];
    *(float*)(preview + PREVIEW_FIT_PREV_SIZE) = *size;
    memcpy(preview + PREVIEW_FIT_PREV_CENTRE, centre, 3 * sizeof(float));
}

static void PreviewFitDetour(uint8_t* preview, uint32_t unit, float dt)
{
    if (!preview) { g_previewFitOriginal(preview, unit, dt); return; }
    const uint32_t item = *(uint32_t*)(preview + PREVIEW_FIT_ITEM);
    if (preview != g_previewFitObject || item != g_previewFitItem) g_previewFitRefits = PREVIEW_FIT_REFITS;
    g_previewFitObject = preview;
    g_previewFitItem = item;
    float* size = (float*)(preview + PREVIEW_FIT_SIZE);
    const float kept = *size;
    const int refit = g_previewFitRefits > 0 && item && *(float*)(preview + PREVIEW_FIT_DELAY) <= 0;
    if (refit) *size = 0;
    g_previewFitOriginal(preview, unit, dt);
    if (!refit) return;
    if (*size == 0) { *size = kept; return; } /* the unit is not up yet: the update stopped before its fit */
    g_previewFitRefits--;
    PreviewFitScale(preview, item);
}

static void InstallPreviewFit(void)
{
    if (g_previewFitState) return;
    uintptr_t game = ApiModuleBase("game.dll", NULL);
    if (strcmp(g_build, "25480438") || !game || !PinMatches(game + PREVIEW_FIT_UPDATE, kPreviewFitEntry, sizeof kPreviewFitEntry) ||
        !PinMatches(game + 0x18EAFF9u, kPreviewFitBlend, sizeof kPreviewFitBlend) ||
        !PinMatches(game + 0x18EAD8Au, kPreviewFitZero, sizeof kPreviewFitZero) ||
        !PinMatches(game + 0x18EBEAFu, kPreviewFitItem, sizeof kPreviewFitItem))
    {
        g_previewFitState = -1;
        Note("preview fit: off; the Armory preview does not match the reviewed build");
        return;
    }
    g_previewFitState = LoaderHook("armory preview fit", game + PREVIEW_FIT_UPDATE, (void*)PreviewFitDetour,
        (void**)&g_previewFitOriginal) ? -1 : 1;
    if (g_previewFitState != 1) Note("preview fit: could not hook the Armory preview's update");
}

static const char* PreviewFitInvalid(uint32_t weapon, const HfPreviewFit* fit)
{
    if (!weapon) return "no weapon";
    if (!fit) return NULL;
    if (fit->size < sizeof(HfPreviewFit)) return "an HfPreviewFit without its size";
    if (fit->scale != 0 && !(fit->scale >= 0.05f && fit->scale <= 20)) return "a scale of 0 or 0.05 .. 20";
    for (int k = 0; k < 3; k++)
        if (!(fit->offset[k] >= -5 && fit->offset[k] <= 5)) return "offsets of -5 .. 5 m";
    return NULL;
}

/* Under the lock: the row for weapon set, or cleared with a NULL fit. 0 done, 1 the table is full. */
static int PreviewFitStore(uint32_t weapon, const HfPreviewFit* fit)
{
    uint32_t i = 0;
    while (i < g_previewFitCount && g_previewFits[i].item != weapon) i++;
    if (!fit)
    {
        if (i < g_previewFitCount) g_previewFits[i] = g_previewFits[--g_previewFitCount];
        return 0;
    }
    if (i == PREVIEW_FIT_MAX) return 1;
    g_previewFits[i] = (PreviewFitRow){weapon, fit->scale, {fit->offset[0], fit->offset[1], fit->offset[2]}};
    if (i == g_previewFitCount) g_previewFitCount++;
    return 0;
}

/* SDK 46, set_preview_fit: weapon is a loadout id (add_weapon's, or a vanilla one); fit NULL or scale 0 clears it. */
static int ApiSetPreviewFit(HfMod* mod, uint32_t weapon, const HfPreviewFit* fit)
{
    const char* why = PreviewFitInvalid(weapon, fit);
    if (why) { ApiLog(mod, "set_preview_fit: refused: %s", why); return 1; }
    AcquireSRWLockExclusive(&g_previewFitLock);
    int full = PreviewFitStore(weapon, fit && fit->scale > 0 ? fit : NULL);
    ReleaseSRWLockExclusive(&g_previewFitLock);
    g_previewFitRefits = PREVIEW_FIT_REFITS; /* the shown item takes it now */
    if (full) ApiLog(mod, "set_preview_fit: refused: %u fits already", PREVIEW_FIT_MAX);
    return full;
}
#define PF_API ApiSetPreviewFit
