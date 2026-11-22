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
| `add_armory_tab` | the Armory screen's tab bar (+0x570): its labels (+0xDFE8, count +0xE068) and selected index (+0xE06C) |

- Effects play in the gameplay world, the Game's +0x10E8. A particles resource must pass the Application table's
  can-get check (+0x388) first, as an unloaded one crashes the engine.
- An effect is created at the node's world pose and linked with the "destroy" orphan policy. The unit's death then
  ends it.
- The Armory's selected id is read as its CUSTOMIZE finds it (0x1458192, 0x18D2270). The item list at the
  screen's +0x7FDE8 holds the selected category and index, each category's count and the ids in one array.
- The link call does not check its unit, so the unit's alive check comes first.
- A shared effect travels on the mod channel under the id `@hellforge.effects`, which no mod id can take. It names
  the entity by network id; the receiver finds it with `entity_of_network` and plays the effect on the game thread.

## The Armory's tab bar

`add_armory_tab` (SDK 26, `armory_tabs.c`) shares the Armory's one tab bar with the game. The bar shows one of four
label lists, and one select function serves them all.

- The bar takes up to 8 text ids through its label setter (0x17AAC50) and its selection through 0x17ABFC0.
- The list on show is found by its labels: the bar's first ids match one of the game's lists (game.dll+0x32E2688,
  +0x33108B0, +0x33111F0, +0x32D4E28). The mods' ids follow.
- The game sets a vanilla list alone when a page opens or closes (0x1457350) and after the weapon grid (0x1457A10).
  HellForge hooks both and puts the mods' labels back before the frame is drawn, keeping the selected index.
- A click or Q/E goes through the bar's update and then the screen's select (0x14570B0) with the bar's index. Past
  the game's labels, that select would read a tab record beyond its own. HellForge's hook returns before it, so the
  game's select never sees a mod's index.
- The callbacks follow the bar's selected index: on a mod's label it is selected, off it or with the Armory closed it
  is not.
- The label setter and the bar's update are left unhooked. The Vanilla Plus Megapack places its own detours there,
  pinned to their first bytes, for its MODS tabs.

## Sight, projectiles, sides and poses

The SDK 27 calls (`world_sight.c`, `joint_poses.c`) replace reads and hooks that several mods had each made.

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
