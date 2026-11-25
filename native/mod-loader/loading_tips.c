/* SDK 51, add_tip and set_text (build 25480438; docs/modding/ui.md). Included after registry.c (AddText, HookText).
 * The tips are 82 text ids at game.dll+0x3317D60 (.data). NextTip (0x187A430) is the only reader: it picks one of the 82
 * (a count in its code, not a field), shows it and waits 15 s. With mod tips or vanilla ones taken out, the detour picks
 * from HellForge's own pool and writes that id into every slot, so the game's pick, layout and timer stay its own. */
#define TIPS_NEXT 0x187A430u
#define TIPS_LIST 0x3317D60u
#define TIPS_VANILLA 82u
#define TIPS_MAX 1024u /* mod tips across all mods */

typedef void (*TipsNextFn)(uint8_t* widget);
static TipsNextFn g_tipsOriginal;
static int g_tipsState; /* 0 not yet, 1 hooked, -1 off */
static SRWLOCK g_tipsLock = SRWLOCK_INIT;
static uint32_t g_tipsVanilla[TIPS_VANILLA]; /* the game's list, read once at the install */
static uint32_t g_tipsAdded[TIPS_MAX], g_tipsAddedCount;
static uint32_t g_tipsRemoved[TIPS_VANILLA], g_tipsRemovedCount;
static uint32_t g_tipsLast;
static uint64_t g_tipsRandom;
static uintptr_t g_tipsGame;

/* 0x187A430: mov [rsp+18h],rbx; push rdi; sub rsp,30h; mov rdi,rcx; mov dword [rcx+0F54h],15.0f */
static const uint8_t kTipsEntry[] = {0x48,0x89,0x5c,0x24,0x18,0x57,0x48,0x83,0xec,0x30,0x48,0x8b,0xf9,0xc7,0x81,0x54,0x0f,0x00,0x00,0x00,0x00,0x70,0x41};
/* 0x187A481: the 82-tip cap and wrap, then lea rcx,[0x3317D60]; mov edx,[rcx+rax*4] */
static const uint8_t kTipsPick[] = {
    0xb8,0x51,0x00,0x00,0x00,0xf2,0x0f,0x59,0x05,0xf2,0xc2,0xb4,0x00,0xf2,0x0f,0x59,0x05,0x8a,0xce,0xb4,0x00,0xf2,0x48,0x0f,
    0x2c,0xc8,0x3b,0xc8,0x0f,0x47,0xc8,0x39,0x8f,0x58,0x0f,0x00,0x00,0x75,0x1a,0xff,0xc1,0x48,0xb8,0x0d,0xce,0xc7,0xe0,0x7c,
    0x0c,0xce,0xc7,0x48,0xf7,0xe1,0x48,0xc1,0xea,0x06,0x48,0x6b,0xc2,0x52,0x48,0x2b,0xc8,0x8b,0xc1,0x89,0x8f,0x58,0x0f,0x00,
    0x00,0x48,0x8d,0x0d,0x8f,0xd8,0xa9,0x01,0x8b,0x14,0x81};

static int TipsIsRemoved(uint32_t id)
{
    for (uint32_t i = 0; i < g_tipsRemovedCount; i++) if (g_tipsRemoved[i] == id) return 1;
    return 0;
}

/* The pool's n-th tip: the vanilla ones not taken out, then the mods'. Under the lock. */
static uint32_t TipsAt(uint32_t n)
{
    for (uint32_t i = 0; i < TIPS_VANILLA; i++)
        if (!TipsIsRemoved(g_tipsVanilla[i]) && n-- == 0) return g_tipsVanilla[i];
    return n < g_tipsAddedCount ? g_tipsAdded[n] : 0;
}

/* A tip from the pool, each equally likely, never the one shown last when there are two or more. Under the lock. */
static uint32_t TipsPick(void)
{
    uint32_t count = g_tipsAddedCount;
    for (uint32_t i = 0; i < TIPS_VANILLA; i++) count += !TipsIsRemoved(g_tipsVanilla[i]);
    if (count == 0) return 0;
    if (g_tipsRandom == 0) g_tipsRandom = (uint64_t)GetTickCount64() * 0x9E3779B97F4A7C15ull | 1;
    for (int tries = 0; tries < 8; tries++)
    {
        g_tipsRandom ^= g_tipsRandom << 13; g_tipsRandom ^= g_tipsRandom >> 7; g_tipsRandom ^= g_tipsRandom << 17;
        const uint32_t id = TipsAt((uint32_t)((g_tipsRandom >> 32) % count));
        if (id != g_tipsLast || count == 1) return g_tipsLast = id;
    }
    return g_tipsLast;
}

static void TipsDetour(uint8_t* widget)
{
    HookText(); /* a tips-only mod's texts: nothing else may have hooked the text lookup yet */
    AcquireSRWLockExclusive(&g_tipsLock);
    const uint32_t id = TipsPick();
    ReleaseSRWLockExclusive(&g_tipsLock);
    volatile uint32_t* list = (volatile uint32_t*)(g_tipsGame + TIPS_LIST); /* .data: the game never writes it */
    for (uint32_t i = 0; id && i < TIPS_VANILLA; i++) list[i] = id;
    g_tipsOriginal(widget);
}

/* From WatchInstalls once the game has loaded: hooks NextTip only once a mod added a tip or took one out. */
static void InstallTips(void)
{
    HookText(); /* set_text's texts, when no other mod text has hooked the lookup yet */
    if (g_tipsState || (g_tipsAddedCount == 0 && g_tipsRemovedCount == 0)) return;
    uintptr_t game = ApiModuleBase("game.dll", NULL);
    if (strcmp(g_build, "25480438") || !game || !PinMatches(game + TIPS_NEXT, kTipsEntry, sizeof kTipsEntry) ||
        !PinMatches(game + 0x187A481u, kTipsPick, sizeof kTipsPick) || SafeRead(game + TIPS_LIST, g_tipsVanilla, sizeof g_tipsVanilla))
    {
        g_tipsState = -1;
        Note("tips: off; the loading screen tips do not match the reviewed build (mods' tips not shown)");
        return;
    }
    g_tipsGame = game;
    g_tipsState = LoaderHook("loading screen tips", game + TIPS_NEXT, (void*)TipsDetour, (void**)&g_tipsOriginal) ? -1 : 1;
    Note(g_tipsState == 1 ? "tips: %u mod tip(s), %u vanilla tip(s) taken out" : "tips: could not hook the tip picker (%u, %u)",
        g_tipsAddedCount, g_tipsRemovedCount);
}

/* SDK 51, set_text: the game's text id answered with the mod's text, in every language. 0 done, 1 refused. */
static int ApiSetText(HfMod* mod, uint32_t id, const char* text)
{
    if (mod == NULL || id == 0 || text == NULL || text[0] == 0) { ApiLog(mod, "set_text: it needs an id and a text"); return 1; }
    if (AddTextAs(text, id) == 0) { ApiLog(mod, "set_text(%08x): no room for more mod texts", id); return 1; }
    HookText();
    return 0;
}

static uint32_t TipTakeOut(HfMod* mod, uint32_t id)
{
    AcquireSRWLockExclusive(&g_tipsLock);
    const int room = TipsIsRemoved(id) || g_tipsRemovedCount < TIPS_VANILLA;
    if (room && !TipsIsRemoved(id)) g_tipsRemoved[g_tipsRemovedCount++] = id;
    ReleaseSRWLockExclusive(&g_tipsLock);
    if (!room) ApiLog(mod, "add_tip: refused: every vanilla tip is out already");
    return room ? id : 0;
}

/* SDK 51, add_tip: a new tip (its text id), a vanilla tip's new text (replaces), or a vanilla tip taken out. 0 refused. */
static uint32_t ApiAddTip(HfMod* mod, const HfTipDef* def)
{
    if (mod == NULL || def == NULL || def->size < sizeof(HfTipDef)) { ApiLog(mod, "add_tip: refused: an HfTipDef without its size"); return 0; }
    if (def->replaces) return def->text == NULL ? TipTakeOut(mod, def->replaces) : ApiSetText(mod, def->replaces, def->text) ? 0 : def->replaces;
    if (def->text == NULL || def->text[0] == 0) { ApiLog(mod, "add_tip: refused: a new tip needs a text"); return 0; }
    if (g_tipsState < 0) { ApiLog(mod, "add_tip: refused: tips are off (modloader.log says why)"); return 0; }
    AcquireSRWLockExclusive(&g_tipsLock);
    char key[160];
    snprintf(key, sizeof key, "%s/tip/%u", mod->id, g_tipsAddedCount);
    const uint32_t id = g_tipsAddedCount < TIPS_MAX ? AddText(def->text, key, 0x52) : 0;
    if (id) g_tipsAdded[g_tipsAddedCount++] = id;
    ReleaseSRWLockExclusive(&g_tipsLock);
    if (!id) ApiLog(mod, "add_tip: refused: no room for more tips");
    else HookText();
    return id;
}
#define TIPS_API ApiSetText, ApiAddTip
