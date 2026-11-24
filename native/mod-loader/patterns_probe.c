/* A log-only look at the hangar's data, once: each unit skin collection's type (the Vehicle Customization category it
 * belongs to), name and skin count, and the local player's picks, which the profile keeps by type. It tells which
 * type a category of HellForge's own may take. Included by patterns.c. */

#define PROBE_PROFILES 0x347CE50u     /* player profiles: 0x1690 bytes each, count at +0x2D200 */
#define PROBE_LOCAL 0x347CEF0u        /* its +0xB398: the local player's id, the first u64 of their profile */

static void PatProbeCollections(uintptr_t game, uint32_t collections)
{
    for (uint32_t k = 0; k < collections && k < PAT_COLLECTION_SLOTS; k++)
    {
        const uint8_t* r = PatRecord(game, k);
        if (r == NULL) continue;
        const uint8_t* skins = *(uint8_t* const*)(r + 0x28);
        uint32_t count = *(const uint32_t*)(r + 0x30);
        Note("hangar probe: collection %u type %u name %016llx skins %u first %08x", k, *(const uint32_t*)(r + 8),
            (unsigned long long)*(const uint64_t*)(r + 0x18), count, skins != NULL && count ? *(const uint32_t*)(skins + 8) : 0u);
    }
}

/* 1 once the local player's picks are logged. */
static int PatProbePicks(uintptr_t game)
{
    const uint8_t* profiles = *(uint8_t* const*)(game + PROBE_PROFILES);
    const uint8_t* local = *(uint8_t* const*)(game + PROBE_LOCAL);
    if (profiles == NULL || local == NULL) return 0;
    uint64_t id = *(const uint64_t*)(local + 0xB398);
    uint32_t n = *(const uint32_t*)(profiles + 0x2D200);
    for (uint32_t i = 0; id != 0 && i < n && i < 8; i++)
    {
        const uint8_t* p = profiles + (size_t)i * 0x1690;
        if (*(const uint64_t*)p != id) continue;
        const uint32_t* at = (const uint32_t*)(p + 0x38 + 0x150);
        Note("hangar probe: %u picks by type: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x",
            *(const uint32_t*)(p + 0x38 + 0x14C), at[0], at[1], at[2], at[3], at[4], at[5], at[6], at[7], at[8], at[9], at[10], at[11]);
        return 1;
    }
    return 0;
}

static void PatProbe(uintptr_t game)
{
    static int listed, picked;
    uint32_t collections = *(volatile uint32_t*)(game + PAT_COLLECTION_COUNT);
    if (picked || collections == 0) return;
    if (!listed) PatProbeCollections(game, collections);
    listed = 1;
    picked = PatProbePicks(game);
}
