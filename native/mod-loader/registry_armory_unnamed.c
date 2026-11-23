/* Game items the armory leaves out: the game's own records in the primary, secondary and throwable lists with no text
 * at all (unfinished items this build ships). Included by registry_load.c, before BuildArmory. */

#define REG_ENCYCLOPEDIA_LOOKUP 0x4F8A50u /* component 76 record of an entity */

#define REG_UNNAMED_MAX 8u /* more game records with no text than this: the text is not there yet, none is left out */

/* Whether a game record's entity has no text at all: no encyclopedia record (component 76), or one with no designation
 * and no name. A record no one entity owns (ArmoryEntity 0) is kept. */
static int Unnamed(uint64_t entity)
{
    if (entity == 0) return 0;
    uint8_t* record = ((EncyclopediaFn)(void*)(g_game + REG_ENCYCLOPEDIA_LOOKUP))(entity);
    if (record == NULL) return 1;
    uint32_t designation, name;
    memcpy(&name, record, 4);
    memcpy(&designation, record + 0x24, 4);
    const char* d = designation ? TextDetour(designation, NULL, NULL, NULL) : NULL;
    const char* m = name ? TextDetour(name, NULL, NULL, NULL) : NULL;
    return (d == NULL || *d == 0) && (m == NULL || *m == 0);
}

/* The game's own records (the first REG_LOADOUT_RECORDS) in the lists the armory shows as tabs (primary, secondary,
 * throwable) whose entity has no text: unfinished game items this build ships (build 25480438: secondary b6ad8730, entity
 * 2ec9cd598e31930f, with the Solo Silo remote's model; throwables 76b384bf, 142fee9d, 50f67c9d). The game's own builder lists them, but no
 * profile can hold them, so the SECONDARY tab showed a nameless pistol with EQUIP (Jan, 23 Nov). The support list (2)
 * stays whole: pickups from a broken hellpod read it (armory_list_test.c). Returns how many are left out. */
static uint32_t UnnamedRecords(uint8_t* table, uint32_t count, uint8_t* left)
{
    static const uint8_t kShown[REG_ARMORY_TYPES] = {[1] = 1, [3] = 1, [4] = 1};
    uint8_t* records = table + (size_t)SlotsOf(REG_LOADOUT) * 16;
    uint32_t found = 0;
    for (uint32_t r = 0; r < count && r < REG_LOADOUT_RECORDS; r++)
    {
        uint32_t type;
        memcpy(&type, records + (size_t)r * REG_LOADOUT_STRIDE + 4, 4);
        if (type >= REG_ARMORY_TYPES || !kShown[type] || !Unnamed(ArmoryEntity(table, r))) continue;
        left[r >> 3] |= (uint8_t)(1u << (r & 7));
        found++;
    }
    return found;
}

/* The records the armory leaves out (UnnamedRecords) as a bitmap in left, logged when they change; NULL when there are
 * more than REG_UNNAMED_MAX (the game's text is not loaded yet: none is left out then). */
static const uint8_t* ArmoryLeftOut(uint8_t* table, uint32_t count, uint8_t left[(REG_LOADOUT_RECORDS + 7) / 8])
{
    memset(left, 0, (REG_LOADOUT_RECORDS + 7) / 8);
    uint32_t unnamed = UnnamedRecords(table, count, left);
    static uint32_t noted = ~0u;
    if (unnamed != noted)
    {
        noted = unnamed;
        if (unnamed > REG_UNNAMED_MAX) Note("registry: %u game items have no text yet; the armory lists them all", unnamed);
        uint8_t* records = table + (size_t)SlotsOf(REG_LOADOUT) * 16;
        for (uint32_t r = 0; unnamed <= REG_UNNAMED_MAX && r < count && r < REG_LOADOUT_RECORDS; r++)
            if (left[r >> 3] & (1u << (r & 7)))
            {
                uint32_t id, type;
                memcpy(&id, records + (size_t)r * REG_LOADOUT_STRIDE, 4);
                memcpy(&type, records + (size_t)r * REG_LOADOUT_STRIDE + 4, 4);
                Note("registry: armory list %u leaves out %08x (entity %016llx): the game has no text for it (an unfinished item)",
                     type, id, (unsigned long long)ArmoryEntity(table, r));
            }
    }
    return unnamed <= REG_UNNAMED_MAX ? left : NULL;
}
