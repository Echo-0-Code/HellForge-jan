/* Weapon paints a mod declares as data (hellforge-data.json "paints"; DataPaints.cs): the plan's paint lines, each added
 * for its mod once the mods are set up, as the mod's add_pattern would. Its colour tables are in the mod's session
 * package, so a data mod's paints need no code. Included by attachments.c after patterns.c (ApiAddPattern). */

#define DP_MAX 256

typedef struct DataPaint
{
    char mod[80], name[96], title[128], description[320];
    uint32_t base;
    HfTextureSwap swaps[HF_PATTERN_SWAPS];
    uint32_t swapCount;
    float colours[HF_PATTERN_COLOURS][3];
    uint32_t colourCount;
    uint32_t weapons[ATT_WEAPONS];
    uint32_t weaponCount;
} DataPaint;

static DataPaint g_dataPaints[DP_MAX];
static int g_dataPaintCount;

/* "a:b;c:d": swaps, 1 when they read. */
static int DpSwaps(char* text, DataPaint* p)
{
    char* context = NULL;
    for (char* s = strtok_s(text, ";", &context); s != NULL; s = strtok_s(NULL, ";", &context))
    {
        char* end = NULL;
        uint64_t from = strtoull(s, &end, 16), to = end != NULL && *end == ':' ? strtoull(end + 1, &end, 16) : 0;
        if (from == 0 || to == 0 || *end || p->swapCount == HF_PATTERN_SWAPS) return 0;
        p->swaps[p->swapCount++] = (HfTextureSwap){from, to};
    }
    return p->swapCount > 0;
}

/* "r,g,b;r,g,b" or "-": the swatch, 1 when it reads. */
static int DpSwatch(char* text, DataPaint* p)
{
    if (!strcmp(text, "-")) return 1;
    char* context = NULL;
    for (char* c = strtok_s(text, ";", &context); c != NULL; c = strtok_s(NULL, ";", &context))
    {
        float* rgb = p->colours[p->colourCount];
        if (p->colourCount == HF_PATTERN_COLOURS || sscanf(c, "%f,%f,%f", &rgb[0], &rgb[1], &rgb[2]) != 3) return 0;
        p->colourCount++;
    }
    return 1;
}

/* "a;b" or "-": the weapons, 1 when they read. */
static int DpWeapons(char* text, DataPaint* p)
{
    if (!strcmp(text, "-")) return 1;
    char* context = NULL;
    for (char* w = strtok_s(text, ";", &context); w != NULL; w = strtok_s(NULL, ";", &context))
    {
        uint32_t id = (uint32_t)strtoul(w, NULL, 10);
        if (id == 0 || p->weaponCount == ATT_WEAPONS) return 0;
        p->weapons[p->weaponCount++] = id;
    }
    return 1;
}

/* paint <mod> <base hex> <name> <title|-> <description|-> <swatch> <swaps> <weapons> (ModLoaderPlan.PaintLine). 1 when
 * it is a paint line; a malformed one is skipped and noted when the mods are set up. */
static int DataPaintPlanLine(char** f, int n)
{
    if (n < 1 || strcmp(f[0], "paint")) return 0;
    if (n != 9 || g_dataPaintCount == DP_MAX) return 1;
    DataPaint* p = &g_dataPaints[g_dataPaintCount];
    memset(p, 0, sizeof *p);
    snprintf(p->mod, sizeof p->mod, "%s", f[1]);
    p->base = (uint32_t)strtoul(f[2], NULL, 16);
    snprintf(p->name, sizeof p->name, "%s", f[3]);
    if (strcmp(f[4], "-")) snprintf(p->title, sizeof p->title, "%s", f[4]);
    if (strcmp(f[5], "-")) snprintf(p->description, sizeof p->description, "%s", f[5]);
    if (p->base && DpSwatch(f[6], p) && DpSwaps(f[7], p) && DpWeapons(f[8], p)) g_dataPaintCount++;
    return 1;
}

static HfMod* DpLoadedMod(const char* id)
{
    for (int k = 0; k < g_modCount; k++) if (!strcmp(g_mods[k].id, id) && g_mods[k].loaded) return &g_mods[k];
    return NULL;
}

static void DpAdd(HfMod* mod, DataPaint* d)
{
    HfPattern p = {sizeof p};
    p.base = d->base, p.name = d->name, p.name_text = d->title[0] ? d->title : NULL, p.description = d->description[0] ? d->description : NULL;
    p.swaps = d->swaps, p.swap_count = d->swapCount;
    p.colours = d->colourCount ? (const float (*)[3])d->colours : NULL, p.colour_count = d->colourCount;
    p.weapons = d->weaponCount ? d->weapons : NULL, p.weapon_count = d->weaponCount;
    if (ApiAddPattern(mod, &p) == 0) Note("paint %s of %s: not added (its log says why)", d->name, d->mod);
}

/* After the mods are set up: each data paint added for its mod, if that mod loaded. */
static void DataPaintsAdd(void)
{
    for (int i = 0; i < g_dataPaintCount; i++)
    {
        HfMod* mod = DpLoadedMod(g_dataPaints[i].mod);
        if (mod != NULL) DpAdd(mod, &g_dataPaints[i]);
        else Note("paint %s: %s did not load, so it is not added", g_dataPaints[i].name, g_dataPaints[i].mod);
    }
}
