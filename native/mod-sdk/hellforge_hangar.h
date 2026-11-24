/* HellForge mod SDK 41: a category of the mod's own in Ship Management > Vehicle Customization (docs/modding/ui.md
 * "A category in Vehicle Customization"). Included by hellforge_lists.h.
 * The game's 5 categories each show one vehicle's patterns as cards. A mod's category shows its own cards there, with
 * the game's cards, EQUIP and equipped mark, and keeps the pick itself: the game's 12 unit pick slots are all in use.
 * The tab is the mod's: a data spec xaml edit adds a 6th TabItem to the view's VehiclesCategoryTabs. */
#ifndef HELLFORGE_HANGAR_H
#define HELLFORGE_HANGAR_H

#include <stdint.h>

#define HF_HANGAR_CATEGORIES 1 /* add_hangar_category exists (another branch may also call itself SDK 41) */

typedef struct HfMod HfMod;

/* Game thread: the player equipped `card` (an id from HfHangarCategory.cards) in the category. */
typedef void (*HfHangarPick)(HfMod* mod, uint32_t category, uint32_t card, void* user);

typedef struct HfHangarCategory
{
    uint32_t size;              /* sizeof(HfHangarCategory) */
    const char* name;           /* the heading above the cards (UTF-8), as a vehicle's name is shown */
    uint32_t count;             /* cards, 1 .. 24 (the screen's 6 rows of 4) */
    const uint32_t* cards;      /* each card's id: a weapon paint's item id, or any id of the mod's other than 0 */
    const char* const* names;   /* each card's name (UTF-8) */
    uint32_t equipped;          /* the card shown as equipped at first; 0 none */
    HfHangarPick on_pick;
    void* user;
} HfHangarCategory;

#endif
