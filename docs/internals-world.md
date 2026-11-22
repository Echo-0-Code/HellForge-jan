# How the world calls work

<!-- diataxis: explanation -->

This page is for people working on HellForge itself. It explains what the SDK 21 calls read and call in the engine.
The mod's side is in [modding/world.md](modding/world.md), and the rest of the mod machinery in
[internals.md](internals.md).

## What each call uses

The SDK 21 calls (`world_calls.c`) call the engine's own C API, the table at game.dll+0x3326308, as the game's Lua
bindings do. Each slot and the first bytes of its function are checked before the first call.

| Call | Reads or calls |
| --- | --- |
| `play_effect` | the World table (+0x8): create particles (+0x278), link them to a unit (+0x338); the Unit table (+0x18): node, pose, alive |
| `stop_effect` | the World table: stop spawning (+0x2A8) or destroy (+0x298). An id that already ended is ignored |
| `find_enemies` | the health manager's records (game.dll+0x3326688) and the faction manager's masks (game.dll+0x3326CB8) |
| `armory_weapon` | the menu object (game.dll+0x347CE38): its state at +0; the customize screen at +0x158, its weapon at +0x10; the Armory at +0x60 |
| `add_armory_tab`, `add_menu_tab` | a menu screen's tab bar: its labels (+0xDFE8, count +0xE068) and selected index (+0xE06C) |

- Effects play in the gameplay world, the Game's +0x10E8. A particles resource must pass the Application table's
  can-get check (+0x388) first, as an unloaded one crashes the engine.
- An effect is created at the node's world pose and linked with the "destroy" orphan policy. The unit's death then
  ends it.
- The Armory's selected id is read as its CUSTOMIZE finds it (0x1458192, 0x18D2270). The item list at the
  screen's +0x7FDE8 holds the selected category and index, each category's count and the ids in one array.
- The link call does not check its unit, so the unit's alive check comes first.
- A shared effect travels on the mod channel under the id `@hellforge.effects`, which no mod id can take. It names
  the entity by network id; the receiver finds it with `entity_of_network` and plays the effect on the game thread.

## The game's tab bars

`add_armory_tab` (SDK 26) and `add_menu_tab` (SDK 27), in `menu_tabs.c`, share the game's tab bars with it. Each bar is
the same widget inside a menu screen, and each screen reacts to a tab change in its own code.

- The bar takes up to 8 text ids through its label setter (0x17AAC50) and its selection through 0x17ABFC0.
- The list on show is found by its labels: the bar's first ids match one of the game's lists. The mods' ids follow.
- Each frame, HellForge finds the screen the menu shows (menu object +0, game.dll+0x347CE38) and puts the mods' labels
  back whenever the game set its list alone. The Armory also sets its lists inside a frame (0x1457350, 0x1457A10), so
  those two are hooked.
- The callbacks follow the bar's selected index: on a mod's label it is selected, off it or with the screen closed it
  is not.

| Bar | Screen (menu +) | Bar in the screen | Vanilla lists (game.dll+) | A mod's index |
| --- | --- | --- | --- | --- |
| Armory | +0x60 | +0x570 | 0x32E2688 (5), 0x33108B0 (3), 0x33111F0 (3), 0x32D4E28 (6) | its select (0x14570B0) would read past its tab records: hooked, returns first |
| Loadout | +0xB0 | +0xF950 | 0x32E26E8 (1 or 2) | its select (0x1470D90) would take it as a page: hooked, returns first |
| Space Station | +0x150 | +0x10F8 | 0x21E9D00 (3) | its update acts on 0 to 2 only |
| Customize | +0x158 | +0x660 + 0x4D0 | 0x21E6738 (2) | its update acts on 0 and 1 only |
| Info Terminal | +0x68 | +0x4D8 | 0x328C9EC (1) | its update ignores tab changes |

- The label setter and the bar's update (0x17AA950) and visual (0x17AA5D0) are left unhooked. The Vanilla Plus
  Megapack places its own detours there, pinned to their first bytes.
- The Escape menu (+0xC8) and Change Bindings (+0xD0) bars, both at +0x4E0, are left out. The Megapack's MODS tabs
  there accept the game's 3 labels or its own 4th, and nothing else.
- The Store (+0x80) labels its bar through another setter (0x17AAFA0) with 12-byte records. The Superstore (+0x90)
  writes the index straight into its page state. Neither is supported yet.

## Where to go next

- [internals.md](internals.md): the rest of how HellForge implements mods.
- [multiplayer.md](multiplayer.md): how co-op works, including the mod channel.
- [sdk-reference.md](sdk-reference.md): every SDK call, component, table and constant.
