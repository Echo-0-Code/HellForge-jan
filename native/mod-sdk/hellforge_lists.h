/* HellForge mod SDK 38 (40: bands): the game's item lists (docs/modding/lists.md). Included by hellforge_mod.h.
 * HellForge owns the code that builds the Armory's weapon grids and the stratagem picker's list. A mod narrows them
 * with a rule (its own test per row) or a filter (text the row's shown name must contain), instead of hooking that
 * code itself: two mods hiding rows in one build no longer fight. */
#ifndef HELLFORGE_LISTS_H
#define HELLFORGE_LISTS_H

#include <stdint.h>

/* The lists (HfListRow.list, and the list argument of the calls) */
#define HF_LIST_ARMORY_WEAPONS 1u /* the Armory's weapon grids: PRIMARY, SECONDARY, THROWABLE and the tabs mods add */
#define HF_LIST_STRATAGEMS 2u     /* the stratagem picker of the loadout screen */

/* One row the game is about to list. Names are UTF-8 in the player's language, as the list shows them. */
typedef struct HfListRow
{
    uint32_t size;        /* sizeof(HfListRow) */
    uint32_t list;        /* HF_LIST_... */
    uint32_t type;        /* the Armory's grid type (1 primary, 3 secondary, 4 throwable) or the row's StratagemType */
    uint32_t item;        /* the Armory row's item id; 0 for a stratagem */
    uint64_t entity;      /* the Armory row's entity; 0 for a stratagem */
    const char* name;     /* "AR-23 Liberator", "Orbital Precision Strike"; "" when it has none */
} HfListRow;

/* add_list_rule: 1 keeps the row, 0 hides it from this build. Runs on the game thread, inside the build; keep it quick
 * and call no list function from it. */
typedef int (*HfListRule)(HfMod* mod, const HfListRow* row, void* user);

/* list_state: what the list showed when it was last built. */
typedef struct HfListState
{
    uint32_t size;        /* sizeof(HfListState) */
    uint32_t open;        /* 1 while the list is on screen */
    uint32_t type;        /* the type it was built for (HfListRow.type; 0 for the stratagem picker) */
    uint32_t total;       /* rows the rules kept */
    uint32_t matched;     /* of those, the rows every filter matches */
    uint32_t shown;       /* rows listed: matched, or total when no row matched (a filter never empties a list) */
    /* SDK 40: the calling mod's band (set_list_band) while the list is up, in the overlay's units; all 0 for none.
     * The layout is 1920 wide and centred: in an overlay draw, x is band_x + (draw->width - 1920) / 2. */
    float band_x;         /* its left edge */
    float band_y;         /* its top edge, down from the screen's top */
    float band_w;         /* its width: the heading's */
    float band_h;         /* its height */
    /* SDK 40: one more each time the game builds the list anew (it opened, or its tab or stratagem slot changed);
     * HellForge's rebuilds for rules and filters do not count. A search box clears its text when it changes. */
    uint32_t opened;
} HfListState;

#include "hellforge_hangar.h" /* SDK 41: a category of the mod's own in Vehicle Customization */
#include "hellforge_loadout.h" /* SDK 42: boxes of mods' own on ARMORY > WEAPONRY */

#endif
