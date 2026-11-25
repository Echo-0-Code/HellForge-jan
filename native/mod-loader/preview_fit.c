/* SDK 46, set_preview_fit (build 25480438; docs/modding/ui.md). Included after loadout_preview.c.
 * The Armory preview's update (0x18EA830) fits its camera to the item's unit box: +0x890 size, +0x87C centre, +0x88C and
 * +0x83C the box it blends from, +0x7E8 the item's loadout id. A unit box smaller than what the model draws shows it too
 * big, so a new box (one this file did not write) is scaled once after the update. Its first frame shows the game's fit. */
#define PREVIEW_FIT_UPDATE 0x18EA830u
#define PREVIEW_FIT_ITEM 0x7E8u
#define PREVIEW_FIT_PREV_CENTRE 0x83Cu
#define PREVIEW_FIT_CENTRE 0x87Cu
#define PREVIEW_FIT_PREV_SIZE 0x88Cu
#define PREVIEW_FIT_SIZE 0x890u
#define PREVIEW_FIT_MAX 128u
#define PREVIEW_FIT_LOGS 32

typedef void (*PreviewFitUpdateFn)(uint8_t* preview, uint32_t unit, float dt);
typedef struct PreviewFitRow { uint32_t item; float scale; float offset[3]; } PreviewFitRow;
static PreviewFitRow g_previewFits[PREVIEW_FIT_MAX];
static uint32_t g_previewFitCount;
static SRWLOCK g_previewFitLock = SRWLOCK_INIT;
static PreviewFitUpdateFn g_previewFitOriginal;
static int g_previewFitState; /* 0 not yet, 1 hooked, -1 off */
static uint8_t* g_previewFitObject;
static uint32_t g_previewFitItem;
static float g_previewFitWritten; /* the size this file last wrote, so it scales each new box once */
static int g_previewFitLogs;

static const uint8_t kPreviewFitEntry[] = {
    0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x78,0x20,0x55,0x48,0x8d,0xa8,0xe8,0xfe,0xff,0xff,0x48,0x81,0xec,0x10,0x02};
/* 0x18EAFF9..0x18EB053: the blend reads the centres (+0x87C/+0x880/+0x884, +0x840/+0x844) and sizes (+0x88C, +0x890). */
static const uint8_t kPreviewFitBlend[] = {
    0xf3,0x0f,0x59,0x97,0x7c,0x08,0x00,0x00,0xf3,0x0f,0x58,0xd0,0x41,0x0f,0x28,0xc6,0xf3,0x0f,0x59,0x87,0x40,0x08,0x00,0x00,
    0xf3,0x0f,0x11,0x55,0x90,0x0f,0x28,0xd1,0xf3,0x0f,0x59,0x97,0x80,0x08,0x00,0x00,0xf3,0x0f,0x58,0xd0,0x41,0x0f,0x28,0xc6,
    0xf3,0x0f,0x59,0x87,0x44,0x08,0x00,0x00,0xf3,0x44,0x0f,0x59,0xb7,0x8c,0x08,0x00,0x00,0xf3,0x0f,0x11,0x54,0x24,0x40,0x0f,
    0x28,0xd1,0xf3,0x0f,0x59,0x97,0x84,0x08,0x00,0x00,0xf3,0x0f,0x59,0x8f,0x90,0x08,0x00,0x00};
/* 0x18EC31F in the preview's set-item (0x18EC2E0): mov [rcx+0x7E8], edx (the item's loadout id). */
static const uint8_t kPreviewFitItem[] = {0x89,0x91,0xe8,0x07,0x00,0x00};

static int PreviewFitFind(uint32_t item, PreviewFitRow* out)
{
    int found = 0;
    AcquireSRWLockShared(&g_previewFitLock);
    for (uint32_t i = 0; i < g_previewFitCount; i++)
        if (g_previewFits[i].item == item) { *out = g_previewFits[i]; found = 1; break; }
    ReleaseSRWLockShared(&g_previewFitLock);
    return found;
}

static void PreviewFitDetour(uint8_t* preview, uint32_t unit, float dt)
{
    g_previewFitOriginal(preview, unit, dt);
    if (!preview) return;
    uint32_t item = *(uint32_t*)(preview + PREVIEW_FIT_ITEM);
    float* size = (float*)(preview + PREVIEW_FIT_SIZE);
    float* prevSize = (float*)(preview + PREVIEW_FIT_PREV_SIZE);
    float* centre = (float*)(preview + PREVIEW_FIT_CENTRE);
    float* prevCentre = (float*)(preview + PREVIEW_FIT_PREV_CENTRE);
    if (preview == g_previewFitObject && item == g_previewFitItem && *size == g_previewFitWritten) return;
    if (*size <= 0) return;
    int fresh = *prevSize == *size && !memcmp(prevCentre, centre, 3 * sizeof(float));
    const float boxSize = *size, box[3] = {centre[0], centre[1], centre[2]};
    PreviewFitRow fit;
    int has = PreviewFitFind(item, &fit);
    if (has)
    {
        /* The camera's distance follows the size, and the item moves by minus the centre. */
        *size = boxSize / fit.scale;
        for (int k = 0; k < 3; k++) centre[k] = box[k] - fit.offset[k];
        if (fresh) { *prevSize = *size; memcpy(prevCentre, centre, 3 * sizeof(float)); }
    }
    if (g_previewFitLogs < PREVIEW_FIT_LOGS && (preview != g_previewFitObject || item != g_previewFitItem))
    {
        g_previewFitLogs++;
        Note("preview fit: item %08x box size %.3f m, centre (%.3f, %.3f, %.3f)%s", item, (double)boxSize, (double)box[0],
            (double)box[1], (double)box[2], has ? ", scaled by its set_preview_fit" : "");
    }
    g_previewFitObject = preview;
    g_previewFitItem = item;
    g_previewFitWritten = *size;
}

static void InstallPreviewFit(void)
{
    if (g_previewFitState) return;
    uintptr_t game = ApiModuleBase("game.dll", NULL);
    if (strcmp(g_build, "25480438") || !game || !PinMatches(game + PREVIEW_FIT_UPDATE, kPreviewFitEntry, sizeof kPreviewFitEntry) ||
        !PinMatches(game + 0x18EAFF9u, kPreviewFitBlend, sizeof kPreviewFitBlend) ||
        !PinMatches(game + 0x18EC31Fu, kPreviewFitItem, sizeof kPreviewFitItem))
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
    g_previewFitWritten = -1; /* the next update fits the shown item again */
    if (full) ApiLog(mod, "set_preview_fit: refused: %u fits already", PREVIEW_FIT_MAX);
    return full;
}
#define PF_API ApiSetPreviewFit
