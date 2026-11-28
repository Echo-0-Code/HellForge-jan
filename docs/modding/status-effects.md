# Status effects of your own

<!-- diataxis: how-to -->

SDK 29 lets a code mod define a status effect of its own: a meter on each entity that fills and falls back, with up
to three levels. A level can swap textures on the entity, scale the damage it takes, and call the mod back.
HellForge keeps the meters and sets the textures, so the look never flickers and a dead entity never keeps it. The
types are in `hellforge_status.h`, which `hellforge_mod.h` includes.

## Contents

- [Before you call](#before-you-call)
- [Define the effect](#define-the-effect)
- [Fill the meter](#fill-the-meter)
- [Give each level a look](#give-each-level-a-look)
- [Read an entity's level](#read-an-entitys-level)
- [Choosing the numbers](#choosing-the-numbers)
- [Change the numbers live](#change-the-numbers-live)
- [Co-op](#co-op)
- [What HellForge guarantees](#what-hellforge-guarantees)

## Before you call

- Check `hf->size >= offsetof(HfApi, effect_state) + sizeof(void*)`; an older HellForge has none of these calls.
- Set `"sdk": 29` in `hellforge-mod.json`.
- Call `add_status_effect` and `add_effect_looks` from `hellforge_mod_init`.

## Define the effect

Zero the struct, set `size`, and fill in the meter and its levels. Each level starts at its `enter` and ends once
the meter falls below its `leave` and its `hold_s` has passed. Levels go down one at a time, so each shows for its
own hold.

```c
static uint32_t g_cryo;

static void Leveled(HfMod* mod, uint32_t effect, uint32_t entity, uint32_t from, uint32_t to, int remote, void* user)
{
    if (to == 3 && !remote) HF_API(mod)->log(mod, "entity %u froze", entity);
}

HfStatusEffect cryo = {sizeof cryo};
cryo.name = "cryo";
cryo.full = 25;           /* the G-23 stun grenade's buildup: a full freeze */
cryo.hold_s = 2;          /* no rewarming for 2 s after the last cold hit */
cryo.decay_per_s = 2.5f;  /* then a tenth of a freeze a second */
cryo.level_count = 3;
cryo.levels[0] = (HfEffectLevel){0.01f, 0.01f, 3, 0, 0};              /* rime: any cold, at least 3 s */
cryo.levels[1] = (HfEffectLevel){10, 5, 0, 0, 0};                     /* iced: from 40 %, until below 20 % */
cryo.levels[2] = (HfEffectLevel){25, 25, 5, 1.2f, HF_EFFECT_SPEND};   /* frozen: 5 s, takes 1.2x damage */
cryo.on_level = Leveled;
g_cryo = api->add_status_effect(mod, &cryo);
```

| Field | What it does |
|---|---|
| `full` | the meter's cap |
| `hold_s` | seconds after the last gain before the meter falls |
| `decay_per_s`, `half_life_s` | linear and exponential loss after the hold; either or both |
| `HfEffectLevel.damage_taken` | multiplies each hit's damage at that level (0: unchanged) |
| `HF_EFFECT_SPEND` | reaching the level empties the meter; gains during its hold are not counted |
| `HF_EFFECT_SHARED_LOOK` | the other players' games show the level too |

The callback runs on the game thread for every level change. With several effects on one entity, their
`damage_taken` multiply.

## Fill the meter

- **From vanilla statuses.** A feed adds `buildup * scale` for every `ApplyStatus` of its `status`. With
  `with_status` set, only hits that bring both statuses to one entity in one game update count. That is how a cryo
  round tells itself apart: it applies the flamer slow (67) and a stun together.
  A status a mod applies itself with `apply_status` feeds no meter.
- **From the mod.** `add_effect_buildup(mod, effect, entity, amount)` adds to the meter, or drains it with a negative
  amount. Call it on the machine that owns the entity (the host, for enemies).

```c
cryo.feed_count = 3;
cryo.feeds[0] = (HfEffectFeed){HF_STATUS_STUN_SMALL, HF_STATUS_FLAMER_SLOWED, 1};
cryo.feeds[1] = (HfEffectFeed){HF_STATUS_STUN_MEDIUM, HF_STATUS_FLAMER_SLOWED, 1};
cryo.feeds[2] = (HfEffectFeed){HF_STATUS_STUN_LARGE, HF_STATUS_FLAMER_SLOWED, 1};
```

## Give each level a look

A look row names one material of one unit and what each level shows on it. It holds the unit's resource
(`UnitResourceName`), the material slot id its meshes give it, and the texture variable to change. Then come the
material's own texture and one texture per level (0: its own). Put the textures in a session package of the mod's
data and pass its id.

```c
static const HfEffectLook kLooks[] = {
    {0x17558b0aa7e7a2a1ull, 0x55bd0b86u, 0xFF2C91CCu, 0x0452abf2bc680514ull, {RIME, ICE, ICE}},
};
api->add_effect_looks(mod, g_cryo, kLooks, sizeof kLooks / sizeof kLooks[0], api->resource_hash("my/mod/textures"));
```

- Key materials by unit and slot, never by shader and slot: a unit's colour variants share those with different
  textures.
- When two effects have a look on one material, the higher level shows.
- What an enemy carries in its mount slots, such as an Automaton's gun, shows the enemy's level. Add rows for the
  carried unit's own resource; the mod loader's log names it the first time an affected enemy carries it.
- Until the package is resident, levels change but no texture is set.

On enemies, use material looks (SDK 33). The game draws units of one kind in one instanced batch with one unit's
material, so a texture set on one bug shows on others of its kind and flickers as the batch order changes.

A row with `texture_slot` 0 names a material resource per level instead (0: the unit's own) and leaves `own` 0. HellForge moves the
slot to that resource (`UnitSetMaterial`), so the enemy is drawn in a batch of its own.

To go back, HellForge sets the material resource the slot had before the first swap. The engine's reset
(`UnitResetMaterial`, `Unit.set_material` with no material) gives the slot an empty material, which draws purple and
see-through. A row whose unit has no drawn material in that slot is not shown.

Make the materials as copies of the unit's own with `textures` in the same session package
([data](data.md#copy)):

```json
{ "type": "material", "copy": "0x10ca31f4d1ba6289", "name": "my/mod/frost/10ca31f4d1ba6289_1",
  "textures": { "0xFF2C91CC": "my/mod/frost/0452abf2bc680514_1" } }
```

## Read an entity's level

`effect_state(mod, effect, entity, &state)` returns the level (0 none) and fills `HfEffectState`: the meter and the
seconds since it last rose. It works on any thread and inside an `on_level` callback.

## Choosing the numbers

Use the game's own buildup scale, with a vanilla item as the yardstick, and derive the timings from the physics of
what the effect is.

| Process | Decay | Why |
|---|---|---|
| a body rewarming, a toxin washing out, an ember cooling | `half_life_s` | first-order: the loss is proportional to what is left (Newton's law of cooling); half-life = 0.69 x the time constant m c / (h A) |
| a surface layer lost at a constant rate (frost sublimating, fuel burning down) | `decay_per_s` | constant flux |
| the lag before the loss starts | `hold_s` | the cold or heat is still at the surface |

Give a level a `leave` below its `enter` when the look has a physical lag, such as ice that forms before it melts.
That also stops a target at the edge from switching looks.

## Change the numbers live

SDK 54: call `add_status_effect` again with the same name to replace the meter, levels, feeds and callback. It answers
the same handle.

- The looks stay, and every entity keeps its meter and level.
- `level_count` stays; a call that changes it is refused.
- It is refused from an `on_level` callback.
- In co-op each player's game uses its own numbers, so tune solo.

## Co-op

Statuses resolve on the entity's owner, so the meters, feeds and damage run there. With `HF_EFFECT_SHARED_LOOK` each
level change goes to the other players by the entity's network id. Their games show the look and call `on_level`
with `remote` 1; gameplay stays on the owner. The other players need the same mod.

A level another game sent is taken over by the game that simulates the entity, as when its owner leaves. Its meter
starts where that level starts, holds for `hold_s`, then decays and is shared from there.

## What HellForge guarantees

- A texture or material is set once, when the shown level changes, and never again while it stays.
- Materials are found again from the live unit each time; no material pointer is kept.
- An entity that dies or loses its unit gets its own textures and materials back while the unit still exists, then is
  forgotten.
- A mod that is switched off loses its effects, and its looks are put back first.
- One `ApplyStatus` hook serves every mod; it only notes the call and passes it on.
