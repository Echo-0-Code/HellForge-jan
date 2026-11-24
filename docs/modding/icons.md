# Item icons

<!-- diataxis: how-to -->

The HUD shows a weapon, tool or backpack as a flat grey silhouette. A copy of a vanilla item keeps its base's. This
page gives an item a silhouette of its own.

## Contents

- [Where the icon comes from](#where-the-icon-comes-from)
- [Draw it](#draw-it)
- [Build it](#build-it)
- [Set it](#set-it)
- [What the loader checks](#what-the-loader-checks)

## Where the icon comes from

An entity's encyclopedia record (`EncyclopediaEntryComponentData`) names its icon at +0x30: a GUI material, which the
HUD draws in grey. The Armory's card pictures are another thing: the game renders those from the item's own unit.

## Draw it

- Side on, the muzzle to the right and the grip down, as the game's own silhouettes.
- One solid shape, white on transparent, with a soft edge of about a pixel. Leave out thin straps and belts.
- Wide, as the game's are: about 2 to 5 times as wide as high.

## Build it

Add an `icon` to your data spec, in the UI icons archive `007e093ca718ca1a` ([data specs](data.md#icon)). An item
icon may be any power-of-two size from 32 to 1024 a side, such as 512 x 128:

```json
{ "archives": ["007e093ca718ca1a"], "resources": [{ "icon": "icons/rifle.png", "name": "yourname/your_mod/icons/rifle", "prepare": "image" }] }
```

## Set it

From `hellforge_mod_init`, set the field on the item:

```c
uint64_t rifle = api->weapon_entity(id);
hf_set(mod, rifle, EncyclopediaEntry, icon, HF_U64(api->resource_hash("yourname/your_mod/icons/rifle")));
```

- A weapon needs its own encyclopedia record: a `name` gives it one, or list `"EncyclopediaEntryComponentData"` in
  `own_components`. An entity of `add_entity` gets one from a `title`.
- The same edit works on a vanilla item. `hf_copy` gives an item another entity's icon.

## What the loader checks

The game crashes on an icon nothing built. So the loader writes only an icon the data spec of a loaded mod (one with code or other edits) builds, one a game
entity already shows, or 0. It refuses anything else and logs why, and the item keeps its icon.
