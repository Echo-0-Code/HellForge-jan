/* SDK 55, set_visibility: how far enemy eyes see one entity, through the scale of its VisibilityComponent record
 * (record +0x8, which the game sets to 1 and never writes again; docs/modding/world.md). A plain write of local memory,
 * no hook: each machine's mod calls it. Removing a record moves another into its slot, so the record is looked up in
 * the manager's map on every call. Included by world_calls.c after behaviours.c (BhvMapFind). */

#define VIS_GET_RVA 0x720530u   /* game.dll: visibility_get, which reads the manager global */
#define VIS_GET_CODE "48895c24084889742410574883ec203b15????????4c8b1d????????743d"
#define VIS_MANAGER 0x3326540u  /* game.dll: the Visibility manager (its map at +0x20, records at +0x40) */
#define VIS_MAP 0x20u
#define VIS_RECORDS 0x40u
#define VIS_STRIDE 0x14u
#define VIS_SCALE 0x8u
#define VIS_HELD 256

typedef struct VisHeld { const HfMod* mod; uint32_t entity; } VisHeld; /* an entity a mod scaled away from 1 */
static VisHeld g_visHeld[VIS_HELD];
static int g_visHeldCount;
static int g_visState; /* 0 not checked, 1 as reviewed, -1 off (logged) */
static uintptr_t g_visGame;

/* The reviewed visibility_get, whose global is the manager at VIS_MANAGER. */
static int VisReady(void)
{
    if (g_visState != 0) return g_visState > 0;
    uintptr_t game = ApiModuleBase("game.dll", NULL);
    int32_t disp = 0;
    int ok = game != 0 && ApiCodeMatches(game + VIS_GET_RVA, VIS_GET_CODE) && !SafeRead(game + VIS_GET_RVA + 24, &disp, 4) &&
        game + VIS_GET_RVA + 28 + (intptr_t)disp == game + VIS_MANAGER;
    g_visState = ok ? 1 : -1;
    g_visGame = game;
    if (!ok) Note("set_visibility: off; visibility_get is not the reviewed one (build 25480438)");
    return ok;
}

/* The scale field of the entity's record, or NULL when it has none (or the manager is not there yet). */
static float* VisScale(uint32_t entity)
{
    uintptr_t manager = *(uintptr_t*)(g_visGame + VIS_MANAGER);
    int64_t index = manager != 0 ? BhvMapFind(manager, VIS_MAP, entity) : -1;
    uint8_t* records = index >= 0 ? *(uint8_t**)(manager + VIS_RECORDS) : NULL;
    return records != NULL ? (float*)(records + (size_t)index * VIS_STRIDE + VIS_SCALE) : NULL;
}

/* Room in the list: the entities whose records are gone (their units died) leave it. */
static void VisPrune(void)
{
    int kept = 0;
    for (int i = 0; i < g_visHeldCount; i++) if (VisScale(g_visHeld[i].entity) != NULL) g_visHeld[kept++] = g_visHeld[i];
    g_visHeldCount = kept;
}

/* The pair noted while its scale is not 1, dropped at 1; entities whose records are gone make room. Game thread. */
static void VisNote(const HfMod* mod, uint32_t entity, int held)
{
    int at = -1;
    for (int i = 0; i < g_visHeldCount && at < 0; i++) if (g_visHeld[i].mod == mod && g_visHeld[i].entity == entity) at = i;
    if (!held) { if (at >= 0) g_visHeld[at] = g_visHeld[--g_visHeldCount]; return; }
    if (at >= 0) return;
    if (g_visHeldCount == VIS_HELD) VisPrune();
    if (g_visHeldCount < VIS_HELD) g_visHeld[g_visHeldCount++] = (VisHeld){mod, entity};
}

/* SDK 55, set_visibility: scale 0.01 .. 10 (above 10: 10, below 0.01: 0.01), 0 back to 1. Game thread. 1 done, 0 not. */
static int ApiSetVisibility(HfMod* mod, uint32_t entity, float scale)
{
    if (mod == NULL || !WcGameThread(mod, "set_visibility")) return 0;
    if (!(scale >= 0)) /* also NaN */
    {
        static volatile LONG told;
        if (!InterlockedExchange(&told, 1)) ApiLog(mod, "set_visibility: refused: scale %g is below 0 (0 puts back the game's 1)", (double)scale);
        return 0;
    }
    if (!VisReady()) return 0;
    float* field = VisScale(entity);
    if (field == NULL) return 0;
    float v = scale == 0 ? 1.0f : scale < 0.01f ? 0.01f : scale > 10.0f ? 10.0f : scale;
    if (*field != v) *field = v;
    VisNote(mod, entity, v != 1.0f);
    return 1;
}

/* A mod taken out (live_code.c ForgetOnGameThread): its entities back at 1. Game thread. */
static void VisForget(const HfMod* mod)
{
    int kept = 0;
    for (int i = 0; i < g_visHeldCount; i++)
    {
        if (g_visHeld[i].mod != mod) { g_visHeld[kept++] = g_visHeld[i]; continue; }
        float* field = g_visState > 0 ? VisScale(g_visHeld[i].entity) : NULL;
        if (field != NULL) *field = 1.0f;
    }
    g_visHeldCount = kept;
}

#define VIS_API ApiSetVisibility
