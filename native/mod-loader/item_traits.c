/* SDK 34: the traits an item's card shows (hellforge_traits.h, docs/modding/traits.md), for game.dll build 25480438.
 * Loadout items: deferred edits of LoadoutEntry traits_1..5, so they are written again on every entity-data load.
 * Stratagem cards: the card builder's two calls are pointed at HellForge, not hooked, so nothing else changes and
 * the row's flags are never written. Included by world_calls.c after ai_orders.c. */
#include "hellforge_traits.h"

#define IT_BUILDER 0x14D97B0u    /* (?, list, StratagemType): the card's ITEM TRAITS lines */
#define IT_FLAG_CALL 0x14D97FFu  /* call +0x11F2680 (type, tag index): the row's flag of that tag */
#define IT_TEXT_CALL 0x14D9888u  /* call +0x1780410 (key): the tag's label, by its text key */
#define IT_FLAG 0x11F2680u
#define IT_TEXT 0x1780410u
#define IT_TEXT_ID 0x17802E0u    /* (id): a text id's text, the game's wrapper of the engine's text call */
#define IT_KEYS 0x32EE650u       /* 25 pointers: each tag's "nui_equipment_tag_*" key, the card's order */
#define IT_TAG_IDS 0x32E5B30u    /* 25 u32: each tag's text id (the HF_ITEM_TRAIT_ ids) */
#define IT_TAGS 25u

/* The builder keeps the key table in r8 across the flag call (its skip path does not reload it). */
static const uint8_t kItBuilderPin[] = {0x48, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x56, 0x41, 0x54};
static const uint8_t kItFlagCallPin[] = {0xE8, 0x7C, 0x8E, 0xD1, 0xFF, 0x84, 0xC0};
static const uint8_t kItTextCallPin[] = {0xE8, 0x83, 0x6B, 0x2A, 0x00, 0x48, 0x8B, 0xD0};
static const uint8_t kItTextIdPin[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x05};
static const uint8_t kItKeysLeaPin[] = {0x4C, 0x8D, 0x05, 0x68, 0x4E, 0xE1, 0x01}; /* at +0x14D97E1: lea r8, [IT_KEYS] */

typedef struct ItCard { HfMod* mod; uint32_t type, count, ids[HF_STRATAGEM_TRAITS_MAX]; } ItCard;
typedef uint8_t (*ItFlagFn)(uint32_t type, uint32_t index);
typedef const char* (*ItTextFn)(const char* key, void* b, void* c, void* d);
typedef const char* (*ItTextIdWrapFn)(uint32_t id);

static SRWLOCK g_itLock = SRWLOCK_INIT;
static ItCard* g_itCards;
static uint32_t g_itCardCount, g_itCardRoom;
static volatile LONG g_itTicking;
static int g_itState; /* 0 not yet, 1 the card's calls point here, -1 off (modloader.log says why) */
static ULONGLONG g_itWaitSince;
static uintptr_t g_itGame;
static HfMod g_itMod;
/* The line the flag call just answered for an override, for the text call that follows it on the same thread. */
static _Thread_local const char* t_itKey;
static _Thread_local uint32_t t_itId;

/* A text id's text, through the game's own id wrapper (the one its menus call). Only from the card's text call: at
 * start-up the engine's text tables are not ready, and asking then crashed in the engine. NULL when it has none. */
static const char* ItTextOfId(uint32_t id)
{
    const char* own = ModTextOf(id);
    if (own != NULL) return own;
    const char* text = ((ItTextIdWrapFn)(g_itGame + IT_TEXT_ID))(id);
    return text != NULL && text[0] ? text : NULL;
}

/* The tag index of a stratagem tag's text id, or -1. */
static int ItTagOf(uint32_t id)
{
    for (uint32_t i = 0; i < IT_TAGS; i++)
    {
        uint32_t tag = 0;
        if (!SafeRead(g_itGame + IT_TAG_IDS + i * 4u, &tag, 4) && tag == id) return (int)i;
    }
    return -1;
}

static const ItCard* ItCardOf(uint32_t type)
{
    for (uint32_t i = 0; i < g_itCardCount; i++) if (g_itCards[i].type == type) return &g_itCards[i];
    return NULL;
}

/* The flag call: an override's lines are tags 0..count-1, so the card shows exactly them, in order. */
static uint8_t ItFlag(uint32_t type, uint32_t index)
{
    t_itKey = NULL;
    AcquireSRWLockShared(&g_itLock);
    const ItCard* card = ItCardOf(type);
    uint32_t count = card != NULL ? card->count : 0, id = card != NULL && index < count ? card->ids[index] : 0;
    ReleaseSRWLockShared(&g_itLock);
    if (card == NULL) return ((ItFlagFn)(g_itGame + IT_FLAG))(type, index);
    if (index >= count) return 0;
    const char* key = NULL;
    if (SafeRead(g_itGame + IT_KEYS + index * 8u, &key, 8) || key == NULL) return 0;
    t_itKey = key;
    t_itId = id;
    return 1;
}

/* The text call: a vanilla tag by its own key (the game's label, whatever its id answers), else the id's text. */
static const char* ItText(const char* key, void* b, void* c, void* d)
{
    ItTextFn text = (ItTextFn)(g_itGame + IT_TEXT);
    if (key == NULL || key != t_itKey) return text(key, b, c, d);
    t_itKey = NULL;
    int tag = ItTagOf(t_itId);
    const char* tagKey = NULL;
    if (tag >= 0 && !SafeRead(g_itGame + IT_KEYS + (uint32_t)tag * 8u, &tagKey, 8) && tagKey != NULL) return text(tagKey, b, c, d);
    const char* own = ItTextOfId(t_itId);
    return own != NULL ? own : text(key, b, c, d);
}

/* A call at `site` pointed at a stub near game.dll that calls `to`; r8 kept when `keep_r8`. 0 done. */
static int ItRedirect(uint8_t* stub, uintptr_t site, void* to, int keep_r8)
{
    uint8_t* c = stub;
    if (keep_r8) { *c++ = 0x41; *c++ = 0x50; *c++ = 0x48; *c++ = 0x83; *c++ = 0xEC; *c++ = 0x20; } /* push r8; sub rsp, 32 */
    *c++ = 0x48; *c++ = 0xB8; memcpy(c, &to, 8); c += 8; /* mov rax, to */
    if (keep_r8) { *c++ = 0xFF; *c++ = 0xD0; *c++ = 0x48; *c++ = 0x83; *c++ = 0xC4; *c++ = 0x20; *c++ = 0x41; *c++ = 0x58; *c++ = 0xC3; }
    else { *c++ = 0xFF; *c++ = 0xE0; } /* jmp rax: the callee returns to the builder */
    int64_t rel = (int64_t)(intptr_t)stub - (int64_t)(site + 5);
    if (rel != (int32_t)rel) return 1;
    uint8_t call[5] = {0xE8};
    int32_t rel32 = (int32_t)rel;
    memcpy(call + 1, &rel32, 4);
    return JournalPut(site, call, sizeof call);
}

static const char* ItInstallAt(uintptr_t game)
{
    if (!PinMatches(game + IT_BUILDER, kItBuilderPin, sizeof kItBuilderPin) || !PinMatches(game + 0x14D97E1u, kItKeysLeaPin, sizeof kItKeysLeaPin) ||
        !PinMatches(game + IT_FLAG_CALL, kItFlagCallPin, sizeof kItFlagCallPin) || !PinMatches(game + IT_TEXT_CALL, kItTextCallPin, sizeof kItTextCallPin) ||
        !PinMatches(game + IT_TEXT_ID, kItTextIdPin, sizeof kItTextIdPin))
        return "the stratagem card's code is not the reviewed one";
    uint8_t* stubs = AllocateNearGame(game, 0x1000);
    if (stubs == NULL) return "no memory within 2 GB of game.dll";
    g_itGame = game;
    uint8_t* textStub = stubs + 64;
    int failed = ItRedirect(stubs, game + IT_FLAG_CALL, (void*)ItFlag, 1);
    DWORD was;
    VirtualProtect(stubs, 0x1000, PAGE_EXECUTE_READ, &was);
    FlushInstructionCache(GetCurrentProcess(), stubs, 0x1000);
    if (failed) return "the card's flag call could not be pointed at HellForge";
    VirtualProtect(stubs, 0x1000, PAGE_EXECUTE_READWRITE, &was);
    failed = ItRedirect(textStub, game + IT_TEXT_CALL, (void*)ItText, 0);
    VirtualProtect(stubs, 0x1000, PAGE_EXECUTE_READ, &was);
    FlushInstructionCache(GetCurrentProcess(), stubs, 0x1000);
    if (failed)
    {
        static const uint8_t back[] = {0xE8, 0x7C, 0x8E, 0xD1, 0xFF};
        JournalPut(game + IT_FLAG_CALL, back, sizeof back); /* both calls or neither: a flag answered with no text of its own */
        return "the card's text call could not be pointed at HellForge";
    }
    return NULL;
}

/* Each override's lines and what they will show, once the card's calls point here. */
static void ItLogLines(void)
{
    for (uint32_t i = 0; i < g_itCardCount; i++)
        for (uint32_t k = 0; k < g_itCards[i].count; k++)
        {
            const uint32_t id = g_itCards[i].ids[k];
            const char* own = ModTextOf(id);
            ApiLog(g_itCards[i].mod, "stratagem traits: type %u line %u: %08x %s%s%s", g_itCards[i].type, k + 1, id,
                own ? "the mod's text \"" : ItTagOf(id) >= 0 ? "the game's tag" : "a game label", own ? own : "", own ? "\"" : "");
        }
}

/* game.dll's code is decoded a while after it loads: tried each update for a minute. */
static void ItTick(HfMod* mod, float dt, void* user)
{
    (void)mod, (void)dt, (void)user;
    if (g_itState != 0 || !g_fired[HF_EVENT_GAME_LOADED]) return;
    uintptr_t game = ApiModuleBase("game.dll", NULL);
    const char* why = strcmp(g_build, "25480438") ? "unsupported game build" : game == 0 ? "game.dll is not loaded" : NULL;
    if (why == NULL && !PinMatches(game + IT_BUILDER, kItBuilderPin, sizeof kItBuilderPin))
    {
        if (g_itWaitSince == 0) g_itWaitSince = GetTickCount64();
        if (GetTickCount64() - g_itWaitSince < 60000) return;
    }
    if (why == NULL) why = ItInstallAt(game);
    g_itState = why ? -1 : 1;
    if (why) { Note("item traits: stratagem cards keep the game's lines: %s", why); return; }
    Note("item traits: stratagem cards on, %u type(s) with lines of their own", g_itCardCount);
    ItLogLines();
}

static void ItStartTick(void)
{
    if (InterlockedExchange(&g_itTicking, 1) != 0) return;
    g_itMod.api = &g_api;
    snprintf(g_itMod.id, sizeof g_itMod.id, "hellforge.item-traits");
    g_itMod.log = g_log;
    g_itMod.loaded = 1;
    ApiOnGameThread(&g_itMod, ItTick, NULL);
}

/* ---- the API ---- */

/* SDK 34, set_item_traits: a loadout item's trait slots, the rest emptied. 0 queued, 1 refused (logged). */
static int ApiSetItemTraits(HfMod* mod, uint64_t entity, const uint32_t* traits, uint32_t count)
{
    const SdkComponent* c = SdkComponentNamed("LoadoutEntry");
    if (mod == NULL || entity == 0 || count > HF_ITEM_TRAITS_MAX || (count != 0 && traits == NULL))
    { ApiLog(mod, "set_item_traits: refused: it needs an entity and at most %u traits", HF_ITEM_TRAITS_MAX); return 1; }
    if (c == NULL || ComponentNamed(c->component) == NULL) { ApiLog(mod, "set_item_traits: this session has no loadout table"); return 1; }
    for (uint32_t i = 0; i < count; i++)
        if (traits[i] == 0) { ApiLog(mod, "set_item_traits(%016llx): trait %u is 0 (add_text refused it?)", (unsigned long long)entity, i + 1); return 1; }
    static const char* const slots[HF_ITEM_TRAITS_MAX] = {"traits_1", "traits_2", "traits_3", "traits_4", "traits_5"};
    for (uint32_t i = 0; i < HF_ITEM_TRAITS_MAX; i++)
    {
        /* an added weapon always has a loadout record of its own: no other component of it is taken */
        FieldEdit e = {mod, EDIT_SET, entity, 0, c, SdkFieldNamed(c, slots[i])};
        uint32_t value = i < count ? traits[i] : 0;
        if (e.field == NULL) { ApiLog(mod, "set_item_traits: HellForge has no mappings for LoadoutEntry.%s", slots[i]); return 1; }
        memcpy(e.value, &value, 4);
        e.vanilla = ModEntity(entity) == NULL;
        if (QueueEdit(e)) return 1;
    }
    ApiLog(mod, "set_item_traits(%016llx): %u trait(s)", (unsigned long long)entity, count);
    return 0;
}

/* Room for one more override; under g_itLock. */
static int ItRoom(void)
{
    if (g_itCardCount < g_itCardRoom) return 1;
    uint32_t room = g_itCardRoom ? g_itCardRoom * 2 : 64;
    ItCard* grown = (ItCard*)malloc(room * sizeof *grown);
    if (grown == NULL) return 0;
    if (g_itCardCount) memcpy(grown, g_itCards, g_itCardCount * sizeof *grown);
    free(g_itCards);
    g_itCards = grown;
    g_itCardRoom = room;
    return 1;
}

/* A type's override set, replaced or (count 0) taken out: 1 done, 0 no room; *other: the mod whose lines it replaced. */
static int ItStore(HfMod* mod, uint32_t type, const uint32_t* traits, uint32_t count, const char** other)
{
    AcquireSRWLockExclusive(&g_itLock);
    ItCard* card = (ItCard*)ItCardOf(type);
    *other = card != NULL && card->mod != mod ? card->mod->id : NULL;
    int done = 1;
    if (count == 0) { if (card != NULL) *card = g_itCards[--g_itCardCount]; }
    else if (card != NULL || (done = ItRoom()) != 0)
    {
        if (card == NULL) card = &g_itCards[g_itCardCount++];
        *card = (ItCard){mod, type, count, {0}};
        memcpy(card->ids, traits, count * 4);
    }
    ReleaseSRWLockExclusive(&g_itLock);
    return done;
}

/* SDK 34, set_stratagem_traits: a stratagem card's lines (count 0: the game's again). 0 done, 1 refused (logged). */
static int ApiSetStratagemTraits(HfMod* mod, uint32_t type, const uint32_t* traits, uint32_t count)
{
    if (mod == NULL || type == 0 || count > HF_STRATAGEM_TRAITS_MAX || (count != 0 && traits == NULL))
    { ApiLog(mod, "set_stratagem_traits: refused: it needs a StratagemType and at most %u lines", HF_STRATAGEM_TRAITS_MAX); return 1; }
    for (uint32_t i = 0; i < count; i++)
        if (traits[i] == 0) { ApiLog(mod, "set_stratagem_traits(%u): line %u is 0 (add_text refused it?)", type, i + 1); return 1; }
    if (g_itState < 0) { ApiLog(mod, "set_stratagem_traits(%u): refused: stratagem cards are off (modloader.log says why)", type); return 1; }
    const char* other = NULL;
    if (!ItStore(mod, type, traits, count, &other)) { ApiLog(mod, "set_stratagem_traits(%u): no room for more", type); return 1; }
    if (other) ApiLog(mod, "set_stratagem_traits(%u): replaces %s's lines (the later call stays)", type, other);
    ApiLog(mod, count ? "set_stratagem_traits(%u): %u line(s)" : "set_stratagem_traits(%u): the game's lines again", type, count);
    if (count) ItStartTick();
    return 0;
}

/* SDK 32's slots (navmesh routes, a pull request of its own): refusals until it is in this build. */
static uint32_t ApiNavRouteOpenAbsent(HfMod* mod, const void* profile) { (void)profile; ApiLog(mod, "nav_route_open: not in this HellForge build"); return 0; }
static int ApiNavRouteAskAbsent(HfMod* mod, uint32_t r, const float* a, const float* b) { (void)mod, (void)r, (void)a, (void)b; return 1; }
static uint32_t ApiNavRouteReadAbsent(HfMod* mod, uint32_t r, const float* a, void* s, float* c, uint32_t m) { (void)mod, (void)r, (void)a, (void)s, (void)c, (void)m; return 0; }
static void ApiNavRouteCloseAbsent(HfMod* mod, uint32_t r) { (void)mod, (void)r; }
/* The table's entries from SDK 32 on (ApiInit). */
#define IT_API {(void*)ApiNavRouteOpenAbsent, (void*)ApiNavRouteAskAbsent, (void*)ApiNavRouteReadAbsent, (void*)ApiNavRouteCloseAbsent}, \
    ApiSetItemTraits, ApiSetStratagemTraits
