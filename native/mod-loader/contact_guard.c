/* A crash of the game's contact points, build 25480438. game.dll 0x6F7820 adds a contact point (8 floats) to its unit's
 * pending list: 64 entries at record +0xD28, the u64 count at +0x1528 right after them, folded and cleared each tick
 * (0x6F9090, which keeps at most 64). A point within 0.5 m of an entry merges into it; any other is appended with no
 * check of the count. A hull as long as a Leviathan's (155 m) makes more than 64 in one tick: the 65th overwrites the
 * count and the next point's merge loop reads past the record (0x6F795A). The cave drops a point that finds the list
 * full, under the game's own lock, so the count stays at 64. Included by customize_guards.c: AllocateNearGame,
 * PinMatches, JournalPut, Note, g_build. */

#define CT_APPEND 0x6F79B8u /* the append: rdx the count, rcx the record, r11 the point; moved into the cave */
#define CT_BACK 0x6F79DCu   /* lock xadd [r9+8], r8: the lock's release, which both paths reach */
#define CT_PIN_AT 0x6F79A7u /* the merge loop's last test, then the append, then the release */
static const uint8_t kCtPin[] = {0x44,0x0f,0x2f,0xcb, 0x77,0x68, 0x41,0xff,0xc2, 0x41,0x8b,0xc2, 0x48,0x3b,0xc2,
    0x72,0x98, 0x41,0x0f,0x28,0x03, 0x41,0x0f,0x28,0x4b,0x10, 0x48,0xc1,0xe2,0x05, 0x0f,0x11,0x84,0x0a,0x28,0x0d,0x00,
    0x00, 0x0f,0x11,0x8c,0x0a,0x38,0x0d,0x00,0x00, 0x48,0xff,0x81,0x28,0x15,0x00,0x00, 0xf0,0x4d,0x0f,0xc1,0x41,0x08};
#define CT_MOVED (CT_BACK - CT_APPEND)
#define CT_LISTED 64u

static int g_ctState; /* 0 waiting for game.dll's code, 1 installed, -1 not */
static ULONGLONG g_ctLoadedAt, g_ctReportedAt;
static volatile LONG g_ctDropped;
static LONG g_ctDroppedReported;

/* cmp rdx, 64; jae full; the moved append; back. full: count it (rax is dead to the return), back. */
static void CtCave(uint8_t* c, uintptr_t game)
{
    static const uint8_t check[] = {0x48,0x83,0xfa,(uint8_t)CT_LISTED, 0x73,(uint8_t)(CT_MOVED + 5)};
    memcpy(c, check, sizeof check); c += sizeof check;
    memcpy(c, kCtPin + (CT_APPEND - CT_PIN_AT), CT_MOVED); c += CT_MOVED;
    c = CgJump(c, game + CT_BACK);
    *c++ = 0x48; *c++ = 0xb8;
    uint64_t counter = (uint64_t)(uintptr_t)&g_ctDropped;
    memcpy(c, &counter, 8); c += 8;
    static const uint8_t count[] = {0xf0,0xff,0x00}; /* lock inc dword [rax] */
    memcpy(c, count, sizeof count); c += sizeof count;
    CgJump(c, game + CT_BACK);
}

static int ContactGuardInstallAt(uintptr_t game)
{
    uint8_t* cave = AllocateNearGame(game, 0x1000);
    if (cave == NULL) return 0;
    CtCave(cave, game);
    DWORD previous;
    VirtualProtect(cave, 0x1000, PAGE_EXECUTE_READ, &previous);
    FlushInstructionCache(GetCurrentProcess(), cave, 0x1000);
    uint8_t jump[CT_MOVED];
    memset(jump, 0x90, sizeof jump); /* never run: nothing jumps into the append, and the cave comes back past it */
    int32_t rel = (int32_t)((intptr_t)cave - (intptr_t)(game + CT_APPEND + 5));
    jump[0] = 0xe9;
    memcpy(jump + 1, &rel, 4);
    return !JournalPut(game + CT_APPEND, jump, sizeof jump);
}

/* After the game loaded, as the customize guards. */
static void ContactGuardInstall(void)
{
    if (g_ctState != 0) return;
    if (strcmp(g_build, "25480438")) { g_ctState = -1; return; }
    uintptr_t game = ApiModuleBase("game.dll", NULL);
    if (game == 0 || !PinMatches(game + CT_PIN_AT, kCtPin, sizeof kCtPin))
    {
        if (g_ctLoadedAt == 0) g_ctLoadedAt = GetTickCount64();
        if (GetTickCount64() - g_ctLoadedAt < 60000) return;
        g_ctState = -1;
        Note("contact guard: not installed; the contact points' code is not the reviewed one");
        return;
    }
    g_ctState = ContactGuardInstallAt(game) ? 1 : -1;
    Note(g_ctState == 1 ? "contact guard: a unit's contact points stop at 64 a tick" : "contact guard: not installed; its code could not be rewritten");
}

/* What it dropped, at most every 10 s. */
static void ContactGuardWatch(void)
{
    ContactGuardInstall();
    if (g_ctState != 1 || GetTickCount64() - g_ctReportedAt < 10000) return;
    LONG dropped = g_ctDropped;
    if (dropped == g_ctDroppedReported) return;
    g_ctReportedAt = GetTickCount64();
    g_ctDroppedReported = dropped;
    Note("contact guard: %ld contact point(s) past a unit's 64 dropped so far", (long)dropped);
}
