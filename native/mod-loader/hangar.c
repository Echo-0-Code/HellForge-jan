/* SDK 41, add_hangar_category and set_hangar_equipped: retired (25 Nov 2026). The 6th Vehicle Customization tab was
 * built from a stale copy of the Ship Management view, and the category hooks (game.dll 0x14DC620 and others) are
 * gone. The two calls stay in HfApi, as every field after them must keep its place, and refuse. Included by
 * world_calls.c. */

static uint32_t ApiAddHangarCategory(HfMod* mod, const HfHangarCategory* def)
{
    (void)def;
    ApiLog(mod, "add_hangar_category: refused: retired; the category is not shown");
    return 0;
}

static int ApiSetHangarEquipped(HfMod* mod, uint32_t category, uint32_t card)
{
    (void)category, (void)card;
    ApiLog(mod, "set_hangar_equipped: refused: retired");
    return 1;
}

#define HG_API ApiAddHangarCategory, ApiSetHangarEquipped
