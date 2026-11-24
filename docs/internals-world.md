# How the world calls work

<!-- diataxis: explanation -->

This page is for people working on HellForge itself. It explains what the SDK 21 and 27 calls read and call in the engine.
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
- The Escape menu (+0xC8) and Change Bindings (+0xD0) bars, both at +0x4E0, take no mod tabs. The Escape bar's 4th
  label is the MODS tab (below). The Megapack's MODS tabs accept the game's 3 labels or a 4th MODS, and nothing else.
- The Store (+0x80) labels its bar through another setter (0x17AAFA0) with 12-byte records. The Superstore (+0x90)
  writes the index straight into its page state. Neither is supported yet.

## The WEAPONRY overview's boxes

`add_loadout_box` and `set_loadout_layout` (SDK 42, `loadout_boxes.c`) add boxes to the overview at Armory screen
+0x44698. Its code holds and counts exactly 3 boxes, so HellForge keeps the extra ones and drives them with the game's
per-box calls.

| Part | Where | Notes |
| --- | --- | --- |
| Boxes | overview +0x330, 3 of 0x3010 | type +0x3000 (1, 3, 4), UI id +0x3004 (+ 0x58EFE12E), highlighted +0x3008 |
| Build | 0x18E18F0 (overview), 0x18E0900 (box) | box 0 is a child of +0x220; box i of box i - 1, anchor (1, 0), x = 14 |
| Set-up | 0x18E1FF0, on each show | label 0x18DE230, item art +0x19D0, preview cell 0x178E170 and 0x18E10B0 |
| Preview | overview +0x9450 slot, +0x9360 cells | a 2 x 3 cell grid; each box takes a whole row, so the extra boxes take a second free slot (+0x728 state) while the overview shows |
| Per frame | 0x1456BB0 | 0x18DC5D0 on box +0x1158; the hover test 0x178F970 covers +0x110 only |
| Selection | +0x9458, 0x18E2F90, 0x18E2DE0 | the game's index stays -1 while an extra box is selected |
| Highlight | 0x18E1150 | animates the box from 326 to 500 wide; `wide_width` and `box_width` replace those |
| Mode | +0x945C | 0 WEAPONRY, 1 APPAREL; the extra boxes show on WEAPONRY only |

- A confirm on an extra box writes index 0, opens the page (0x1457350) and picks the box's mod tab on the bar.
- The overview is built with the Armory screen. If that happened before the hooks, the first update builds the boxes.

## The escape menu's MODS tab

`mods_tab.c` and `mods_tab_model.c` show the `add_option` (SDK 30) rows in a MODS tab after GAME, SOCIAL and OPTIONS.
They port the Vanilla Plus Megapack's Mod Options Menu (0BSD) and keep its review of every offset and function.

- The tab is a 4th label set through the label setter, which is called and never hooked. The game draws it and cycles
  Q/E over it.
- On MODS, the OPTIONS content shows with its category buttons relabelled, one per mod. The shown panel's rows are
  rebuilt from HellForge's own setting descriptors.
- Every row names setting 139, whose value type is none, so no game setting is read or written.
- Edits wait for APPLY (Tab), which saves them and calls each mod's `on_change`. Leaving with edits opens the game's
  UNAPPLIED CHANGES dialog, whose confirm discards them.
- Past 8 mods, the buttons show 7 at a time and the 8th turns the pages.
- It waits up to 60 s after the game loads for the 25 functions, the tab and category labels and setting 139.

It steps aside so the menu never shows two MODS tabs:

- When the Megapack is loaded and its `mod_options_menu` component is on (its `settings.ini`), that menu draws the tab.
  HellForge's rows reach it through the bundled HellForge Mod Options Lua mod.
- When the bar already has a 4th tab that is not HellForge's, its tab stays off for the session.
- `modloader.log` says which with a `mods tab:` line.

## Sight, projectiles, sides and poses

The SDK 28 calls (`world_sight.c`, `joint_poses.c`) replace reads and hooks that several mods had each made.

| Call | Reads or calls |
| --- | --- |
| `line_of_sight` | the exe's world id (0x79F860) of the physics world (game.dll+0x346BFA0), then its raycast (0x7F4590) |
| `find_projectiles`, `set_projectile` | the projectile pool (game.dll+0x347CEA8): flags +0x203C, flights +0x3040, sources +0x3B040, hits +0x4D040, types +0xE5040 |
| `faction_of`, `side_of`, `find_entities` | the faction and health managers, as `find_enemies` reads them |
| `on_render`, `pose_joint`, `joint_pose` | the engine's script render step (exe 0x689BC0), set_local_pose (0x1FCAD0), update_unit (0x1B2DC0) and the Unit calls |

- The raycast takes the game's own preset for its filter (exe+0x27B9440 entry 1: 2, 5, 0x393D9518, 0x80000009).
  Other values return no hits at all.
- The raycast calls through the world's scene unchecked (exe+0x27BA870, 0xB0 a world, the scene at +0x20). A world
  with no scene would crash it, so the scene is read first.
- The AI casts its rays as 24 jobs over a batch (game.dll+0x347D7E0, rows at +0x40008). A ray is cast only when every
  row reads done, as they share the scene. Rays merely queued for the next kick do not matter.
- A hit record is 44 bytes: the distance at +24, the unit at +28.
- A pool slot is in flight when its flags read 2 (in use, not ended) and its hit record's impact flag (+0xB9) is 0.
  The hit record's +0 is the peer credited.
- Sides follow the AI's perception gather (0x887270): a mask sharing a bit with the seer's is friendly, no mask is
  neutral, and any other mask is hostile.
- The render step is where the game has animated every unit and the frame has not rendered yet. The terminal camera
  hooks it for the same reason. Poses queued outside a callback are set there first, then the callbacks run, then each
  touched unit is updated once.

## Where to go next

- [internals.md](internals.md): the rest of how HellForge implements mods.
- [multiplayer.md](multiplayer.md): how co-op works, including the mod channel.
- [sdk-reference.md](sdk-reference.md): every SDK call, component, table and constant.
