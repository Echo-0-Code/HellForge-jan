/* Paints on a throwable copy (HfWeapon.paints, docs/modding/patterns.md). Included by registry_load.c, after
 * OwnCustomization. */

/* A painted vanilla secondary's entity, the donor of a throwable's customization record (PaintWeapons gave it one). */
static uint64_t PaintDonor(const WeaponLoad* L)
{
    for (uint32_t r = 0; r < L->recordCount; r++)
    {
        uint32_t id, type;
        memcpy(&id, L->records + (size_t)r * REG_LOADOUT_STRIDE, 4);
        memcpy(&type, L->records + (size_t)r * REG_LOADOUT_STRIDE + 4, 4);
        for (int i = 0; type == 3 && i < g_paintCount; i++)
            if (g_painted[i] == id) return LoadoutOwner(L->index, r);
    }
    return 0;
}

/* A throwable copy with HfWeapon.paints: the game gives grenades no customization record, and the customize screen needs
 * one naming a paint package (0x1493770). So its row names a painted secondary's record, which it then owns, emptied
 * down to the paint slot, the Default paint and a package of its own. The entity's component list stays its base's, so
 * nothing changes in a mission. NULL when it is not one, with the reason logged. */
static uint8_t* OwnThrowablePaints(const WeaponLoad* L, const Weapon* w, uint64_t baseEntity)
{
    uint8_t* table = StatePointer(L->state, REG_TABLES + 8 * REG_CUSTOMIZATION);
    uint32_t slots = SlotsOf(REG_CUSTOMIZATION);
    if (!w->paints || w->plain || table == NULL || slots == 0 || FindRow(table, slots, 16, baseEntity) != NULL) return NULL;
    const uint64_t donor = PaintDonor(L);
    uint8_t* donorRow = donor != 0 ? FindRow(table, slots, 16, donor) : NULL;
    uint8_t* slot = FindRow(table, slots, 16, w->entity) == NULL ? FreeRow(table, slots, 16, w->entity) : NULL;
    if (donorRow == NULL || slot == NULL || g_paintCount >= REG_MAX_PAINTED)
    {
        ApiLog(w->mod, "%s: no paints (%s)", w->name, donorRow == NULL ? "no painted secondary to copy a record from" : "no room");
        return NULL;
    }
    uint8_t entry[16];
    memcpy(entry, &w->entity, 8);
    memcpy(entry + 8, donorRow + 8, 4);
    memset(entry + 12, 0, 4);
    RegisterRow(L, slot, entry, 16);
    uint8_t* record = OwnRecord(L->state, REG_CUSTOMIZATION, REG_CUSTOMIZATION_STRIDE, donor, w->entity);
    if (record == NULL) { ApiLog(w->mod, "%s: no paints (its customization record could not be made)", w->name); return NULL; }
    const uint32_t none = 0;
    memset(record, 0, REG_SLOTS_AT + 10 * 4);          /* the donor's defaults and slots */
    memset(record + REG_OVERRIDES_AT, 0, REG_OVERRIDES_BYTES);
    memcpy(record + REG_MASTERY_PACKAGE_AT, &none, 4);
    if (!GivePaintSlot(record, w->id)) return NULL;
    g_painted[g_paintCount++] = w->id;
    uint32_t package;
    memcpy(&package, record + REG_MASTERY_PACKAGE_AT, 4);
    ApiLog(w->mod, "%s: a customization record of its own with the paint slot and paint package %u", w->name, package);
    return record;
}
