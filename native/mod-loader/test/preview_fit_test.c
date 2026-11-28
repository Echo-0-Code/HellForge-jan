/* The Armory preview's fit (preview_fit.c) over a stand-in for the game's update, which fits to its own box on a size
 * of 0 and on a change of view kind, as the game does. A paint picked in CUSTOMIZE changes the kind: the item must
 * keep its set_preview_fit, on the new kind's box. Included by stats_test.c. */

static float g_pftBox[8]; /* the game's box size by view kind */
static void PftUpdate(uint8_t* preview, uint32_t unit, float dt)
{
    (void)unit, (void)dt;
    uint32_t* asked = (uint32_t*)(preview + PREVIEW_FIT_KIND);
    uint32_t* shown = (uint32_t*)(preview + 0x7D8);
    float* size = (float*)(preview + PREVIEW_FIT_SIZE);
    if (*size == 0 || *asked != *shown)
    {
        const float centre[3] = {0, 0, 1};
        *size = g_pftBox[*asked & 7];
        memcpy(preview + PREVIEW_FIT_CENTRE, centre, sizeof centre);
    }
    *shown = *asked;
}

static void PreviewFitFixture(void)
{
    static HfMod mod;
    static uint8_t preview[0x900];
    const uint32_t item = 0x1234u, savedFits = g_previewFitCount, savedSeen = g_previewFitSeenCount;
    const PreviewFitUpdateFn savedOriginal = g_previewFitOriginal;
    const int savedLogs = g_previewFitLogs;
    float* size = (float*)(preview + PREVIEW_FIT_SIZE);
    const float* centre = (const float*)(preview + PREVIEW_FIT_CENTRE);
    HfPreviewFit fit = {sizeof fit, 0.5f, {0, 0, 2}, 0, {0, 0, 0}};
    g_previewFitOriginal = PftUpdate;
    g_previewFitObject = NULL;
    g_pftBox[0] = 4, g_pftBox[3] = 2;
    memcpy(preview + PREVIEW_FIT_ITEM, &item, 4);
    assert(ApiSetPreviewFit(&mod, item, &fit) == 0);

    for (int i = 0; i < PREVIEW_FIT_REFITS; i++) PreviewFitDetour(preview, 0, 0.016f);
    assert(g_previewFitRefits == 0 && *size == 8 && centre[2] == -1); /* half as big, 2 m down */
    PreviewFitDetour(preview, 0, 0.016f);
    assert(g_previewFitRefits == 0 && *size == 8);

    /* a paint picked: the game fits to kind 3's box, a smaller one, and the item's fit goes onto that */
    *(uint32_t*)(preview + PREVIEW_FIT_KIND) = 3;
    PreviewFitDetour(preview, 0, 0.016f);
    assert(g_previewFitRefits == PREVIEW_FIT_REFITS - 1 && g_previewFitKind == 3 && *size == 4 && centre[2] == -1);
    while (g_previewFitRefits > 0) PreviewFitDetour(preview, 0, 0.016f);
    assert(*size == 4 && centre[2] == -1 && g_previewFitSeenCount == savedSeen + 2); /* a box kept per kind */
    *(uint32_t*)(preview + PREVIEW_FIT_KIND) = 0;
    PreviewFitDetour(preview, 0, 0.016f);
    assert(g_previewFitRefits == PREVIEW_FIT_REFITS - 1 && *size == 8);

    assert(ApiSetPreviewFit(&mod, item, NULL) == 0 && g_previewFitCount == savedFits);
    g_previewFitSeenCount = savedSeen, g_previewFitRefits = 0, g_previewFitObject = NULL, g_previewFitLogs = savedLogs;
    g_previewFitOriginal = savedOriginal;
}
