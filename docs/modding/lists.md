# Item lists

<!-- diataxis: how-to -->

SDK 38 lets a mod narrow two of the game's lists without hooking the code that builds them: the Armory's weapon
grids and the stratagem picker. HellForge owns that code. A mod adds a rule (its own test per row) or a filter (text
the row's name must contain). The types are in `hellforge_lists.h`, which `hellforge_mod.h` includes.

## Contents

- [Before you call](#before-you-call)
- [Hide rows with a rule](#hide-rows-with-a-rule)
- [Filter by name](#filter-by-name)
- [Read what the list shows](#read-what-the-list-shows)
- [Make room above the list](#make-room-above-the-list)
- [What HellForge does in a build](#what-hellforge-does-in-a-build)

## Before you call

- Check `hf->size >= offsetof(HfApi, list_state) + sizeof(void*)` and set `"sdk": 38` in `hellforge-mod.json`.
- The lists are `HF_LIST_ARMORY_WEAPONS` and `HF_LIST_STRATAGEMS`.
- Do not hook `0x18D7360` (the Armory grid) or `0x18D8710` (the picker) to hide rows. Two mods writing the same rows in
  one build undo each other's work.
- Everything is client side. Nothing is sent to the other players.

## Hide rows with a rule

A rule answers 1 to keep a row and 0 to hide it. Every build of the list asks it of each row the game would list.

```c
static int MeleeOnly(HfMod* mod, const HfListRow* row, void* user)
{
    if (row->type != 3) return 1;               /* only the SECONDARY grid */
    return IsMelee(row->item) == g_meleeTabPicked;
}

hf->add_list_rule(mod, HF_LIST_ARMORY_WEAPONS, MeleeOnly, NULL);
```

- Add rules from `hellforge_mod_init`. A rule cannot be taken away; make it keep every row when it has nothing to do.
- A rule runs on the game thread, inside the build. Keep it quick, and call no list function from it.
- When what your rule decides changes, call `rebuild_list`.

| `HfListRow` field | Armory | Stratagem picker |
|---|---|---|
| `type` | the grid type: 1 primary, 3 secondary, 4 throwable | the StratagemType |
| `item` | the row's item id | 0 |
| `entity` | the row's entity | 0 |
| `name` | "AR-23 Liberator" | "Orbital Precision Strike" |

Names are UTF-8 in the player's language, as the list shows them, and include mod texts.

## Filter by name

```c
hf->set_list_filter(mod, HF_LIST_STRATAGEMS, "orbital");  /* only rows whose name contains it, any case */
hf->set_list_filter(mod, HF_LIST_STRATAGEMS, "");         /* the mod's filter taken away */
```

- Call it from any thread, for example on every key a search box takes. The list rebuilds on the next update when it
  is up.
- Each mod has one filter per list. A row shows only when every mod's filter matches it.
- A filter that matches no row is set aside for that build: the list shows what the rules keep. An empty grid is not a
  state the game's pages handle. `list_state` tells you that nothing matched.
- Clear your filter when the list closes, or the next time it opens it is still narrowed.

## Read what the list shows

```c
HfListState state = {sizeof state};
if (hf->list_state(mod, HF_LIST_ARMORY_WEAPONS, &state) && state.open)
    snprintf(hint, sizeof hint, state.matched ? "%u of %u" : "no match", state.matched, state.total);
```

| Field | Meaning |
|---|---|
| `open` | 1 while the list is on screen |
| `type` | the grid type it was built for (0 for the picker) |
| `total` | rows the rules kept |
| `matched` | of those, rows every filter matches |
| `shown` | rows listed: `matched`, or `total` when nothing matched |

## Make room above the list

SDK 40 moves the list's heading and rows down by a band the mod draws in, such as a search box. The list's bottom stays.

```c
hf->set_list_band(mod, HF_LIST_ARMORY_WEAPONS, 60.0f);   /* 60 units; 0 takes it away */

/* in the overlay draw */
HfListState state = {sizeof state};
if (hf->list_state(mod, HF_LIST_ARMORY_WEAPONS, &state) && state.open && state.band_h > 0)
{
    const float x = state.band_x + (draw->width - 1920.0f) / 2.0f;
    draw->rect(x, state.band_y, x + state.band_w, state.band_y + state.band_h, fill, edge, 2);
}
```

- Units are the overlay's: 1080 high. The game's layout is 1920 wide and centred, hence the `x` offset.
- Each mod's band is its own slice, in the order the mods asked; the total is at most 200 units.
- HellForge gives the band again after each build of the list, such as a tab change.
- Take it away when the list closes, or it is still there when the list opens again.
- Check `hf->size >= offsetof(HfApi, set_list_band) + sizeof(void*)` and set `"sdk": 40`.

## What HellForge does in a build

- **Armory.** The builder skips a row whose item is 0. HellForge writes 0 into the hidden rows while it runs and puts
  the items back as it returns. A rebuild goes through the page's own tab select, as a tab change does.
- **Stratagem picker.** Before the walk over the types, HellForge decides which held types to hide. The walk's "held?"
  call points at HellForge, which answers no for those. Empty headings hide themselves. A rebuild calls the picker's
  builder again with the arguments of its last build, while the picker is up.
- Both are refused on another game build or other code at those places. `modloader.log` then says why, and the lists
  stay as the game builds them.
