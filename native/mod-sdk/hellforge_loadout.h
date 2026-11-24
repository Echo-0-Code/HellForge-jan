/* HellForge mod SDK 42: more boxes on ARMORY > WEAPONRY (docs/modding/ui.md "Boxes on the WEAPONRY overview").
 * Included by hellforge_lists.h. The overview shows the game's PRIMARY, SECONDARY and THROWABLE boxes; a mod adds
 * boxes of its own after them, built by the game's own box code, and a click on one opens the mod's armory tab. */
#ifndef HELLFORGE_LOADOUT_H
#define HELLFORGE_LOADOUT_H

#include <stdint.h>

#define HF_LOADOUT_BOXES 1   /* add_loadout_box exists (another branch may also call itself SDK 42) */
#define HF_LOADOUT_EXTRA 3u  /* boxes all mods together can add: one preview slot holds 3 weapon rows */

typedef struct HfMod HfMod;

/* Game thread, each time the overview sets its boxes up: the item the box shows (a loadout item id), 0 for none. */
typedef uint32_t (*HfLoadoutBoxItem)(HfMod* mod, uint32_t box, void* user);

typedef struct HfLoadoutBoxDef
{
    uint32_t size;           /* sizeof(HfLoadoutBoxDef) */
    const char* label;       /* the heading, such as "MELEE" (UTF-8) */
    uint32_t tab;            /* the mod's add_armory_tab handle on HF_ARMORY_TABS_WEAPONS that a click opens */
    HfLoadoutBoxItem item_fn;
    void* user;
} HfLoadoutBoxDef;

/* set_loadout_layout: 0 in a field keeps the game's value. */
typedef struct HfLoadoutLayout
{
    uint32_t size;           /* sizeof(HfLoadoutLayout) */
    uint32_t columns;        /* boxes in a row, 3 .. 6; the rest go in a second row below (0: one row) */
    float box_width;         /* a box's width in units (the game's: 326) */
    float box_height;        /* a box's height (the game's: 770) */
    float gap;               /* space between boxes (the game's: 14) */
    float wide_width;        /* the selected box's width (the game's: 500) */
} HfLoadoutLayout;

#endif
