# Item traits

<!-- diataxis: how-to -->

SDK 34 lets a code mod choose the trait lines an item's card shows, with labels of its own. One kind of trait serves
every card: a text id. Use one of the game's labels (the `HF_ITEM_TRAIT_` ids in `hellforge_traits.h`) or a text of
your own from `add_text`. Include `hellforge_traits.h` beside `hellforge_mod.h`.

## Contents

- [Before you call](#before-you-call)
- [Which call for which item](#which-call-for-which-item)
- [Give a loadout item its traits](#give-a-loadout-item-its-traits)
- [Give a stratagem card its lines](#give-a-stratagem-card-its-lines)
- [The game's labels](#the-games-labels)
- [What HellForge changes in the game](#what-hellforge-changes-in-the-game)

## Before you call

- Check `hf->size >= offsetof(HfApi, set_stratagem_traits) + sizeof(void*)`; an older HellForge has neither call.
- Set `"sdk": 34` in `hellforge-mod.json`.
- Either call works any time; call them from `hellforge_mod_init` so the first card already has them.

## Which call for which item

| Item | Card | Call |
|---|---|---|
| Primary, secondary, throwable | The armory's WEAPON TRAITS box | `set_item_traits(mod, entity, ids, count)` |
| Support weapon, backpack | The armory card (its loadout record) | `set_item_traits` |
| Any stratagem: support weapon, backpack, sentry, emplacement, mine, orbital, Eagle, vehicle | The stratagem card's ITEM TRAITS box | `set_stratagem_traits(mod, type, ids, count)` |

A support weapon or backpack has both cards: give it both calls.

## Give a loadout item its traits

```c
uint32_t cryo = hf->add_text(mod, "trait_cryo", "CRYO");
const uint32_t rifle[] = {HF_ITEM_TRAIT_LIGHT_AP, cryo};
hf->set_item_traits(mod, hf->weapon_entity(rifleId), rifle, 2);
```

- `entity` is the item's entity resource: an `HF_ENTITY_` constant, or `weapon_entity` for one `add_weapon` added.
- Up to 5 ids. The item's other slots are emptied, so list every trait it should show, handling traits included.
- HellForge writes the slots again whenever the game reloads its entity data. A later mod's call for the same item wins.

## Give a stratagem card its lines

```c
const uint32_t crawler[] = {HF_ITEM_TRAIT_SENTRY, hf->add_text(mod, "trait_mobile", "MOBILE"), HF_ITEM_TRAIT_MEDIUM_AP};
hf->set_stratagem_traits(mod, crawlerType, crawler, 3);
```

- `type` is any StratagemType: an `HF_STRATAGEM_` constant or one `add_stratagem` gave.
- Up to 4 lines, the box's own limit, shown in your order. `count` 0 gives the card the game's lines again.
- The stratagem's flags (`HfStratagem.flags`) stay as they are. Keep them true too: the game uses them for more than
  the card.
- Without a call, the card shows the game's lines: the first 4 set flags in the game's tag order.

## The game's labels

Every `HF_ITEM_TRAIT_` id fits either card. The first group are the stratagem card's 25 tags. A stratagem card shows
those through the game's own tag text. The second group are labels only weapons carry: `EXPLOSIVE`, `INCENDIARY`,
`LIGHT_ANTI_TANK`, `STIMULATIVE`, `ROUNDS_RELOAD`, `SUPPRESSED` and `TEAM_WEAPON`.

Pick the armour-penetration label by the item's armour value: 2 light, 3 medium, 4 heavy, 5 light anti-tank, 6 and
up anti-tank. The label is text only; the damage row decides what a hit penetrates.

## What HellForge changes in the game

- Loadout items: deferred edits of `LoadoutEntryComponentData` `traits_1` to `traits_5`, as `hf_set` makes.
- Stratagem cards: the card builder's two calls, the flag test and the label text, point at HellForge. Types with no
  lines of a mod's go to the game's own code. Nothing else calls through them, and no flag is written.
- modloader.log says `item traits: stratagem cards on` once installed. Each mod's log lists its lines and their text.
