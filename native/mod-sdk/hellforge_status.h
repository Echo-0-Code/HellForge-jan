/* HellForge mod SDK 29: status effects of a mod's own (add_status_effect, docs/modding/status-effects.md).
 * Included by hellforge_mod.h. An effect is a meter per entity, filled by vanilla statuses or by the mod, that falls
 * after a hold; its levels change what the entity looks like and how much damage it takes. HellForge sets a level's
 * textures once, when the level changes, and puts the entity's own back when it ends or the entity dies. */
#ifndef HELLFORGE_STATUS_H
#define HELLFORGE_STATUS_H

#include <stdint.h>

#define HF_EFFECT_LEVELS 3u /* levels above 0 an effect may have */
#define HF_EFFECT_FEEDS 8u  /* vanilla statuses that may feed one effect */

/* HfStatusEffect.flags */
#define HF_EFFECT_SHARED_LOOK 0x1u /* level changes go to the other players, so their games show the look too */
/* HfEffectLevel.flags */
#define HF_EFFECT_SPEND 0x1u /* reaching the level empties the meter; gains during its hold_s are not counted */

/* A vanilla status that fills the meter: each ApplyStatus of `status` on an entity this machine owns adds its buildup
 * times scale. with_status (0: none) must land on the same entity in the same game update for it to count, as a
 * cryo round's slow and stun do. */
typedef struct HfEffectFeed
{
    uint32_t status;      /* HF_STATUS_... (its buildup is what counts) */
    uint32_t with_status; /* 0, or the status it must arrive with */
    float scale;          /* meter per buildup, 0.001 .. 1000 */
} HfEffectFeed;

/* A level: from `enter` up, until the meter falls below `leave` (at most enter: no flicker at the edge) and at least
 * hold_s after it started. damage_taken scales the damage the entity takes at this level (0 or 1: unchanged). */
typedef struct HfEffectLevel
{
    float enter, leave;   /* meter, above 0 and at most full; each level's enter above the one before */
    float hold_s;         /* 0 .. 600 */
    float damage_taken;   /* 0.1 .. 10, or 0 */
    uint32_t flags;       /* HF_EFFECT_SPEND */
} HfEffectLevel;

/* Every level change of an entity, on the game thread: from and to are levels (0 none). remote 1 when the change came
 * from another player's game (HF_EFFECT_SHARED_LOOK): only the look follows it there. */
typedef void (*HfEffectCallback)(HfMod* mod, uint32_t effect, uint32_t entity, uint32_t from, uint32_t to, int remote, void* user);

typedef struct HfStatusEffect
{
    uint32_t size;        /* sizeof(HfStatusEffect) */
    const char* name;     /* unique within the mod; the other players' games find the effect by mod id and name */
    float full;           /* the meter's cap, 0.01 .. 100000 */
    float hold_s;         /* no loss for this long after the last gain, 0 .. 600 */
    float decay_per_s;    /* then this much a second (0: none) ... */
    float half_life_s;    /* ... and half of what is left every half_life_s (0: none) */
    uint32_t level_count; /* 1 .. HF_EFFECT_LEVELS */
    HfEffectLevel levels[HF_EFFECT_LEVELS];
    uint32_t feed_count;  /* 0 .. HF_EFFECT_FEEDS (0: only add_effect_buildup fills it) */
    HfEffectFeed feeds[HF_EFFECT_FEEDS];
    HfEffectCallback on_level; /* may be NULL */
    void* user;
    uint32_t flags;       /* HF_EFFECT_SHARED_LOOK */
} HfStatusEffect;

/* A look: one material of a unit (its UnitResourceName and the slot id its meshes give the material), its own texture
 * in a texture slot, and the texture each level shows there (0: its own). SDK 33: texture_slot 0 makes it a material
 * look: each level names a material resource the slot takes (0: the unit's own), and own is unused (HellForge puts the
 * slot back on the resource it had before the first swap; the engine's reset draws an empty material). Use material
 * looks on enemies: the game draws a kind's units in one batch with one unit's material, so a texture set on one
 * enemy shows on others of its kind and flickers. */
typedef struct HfEffectLook
{
    uint64_t unit;
    uint32_t material_slot;
    uint32_t texture_slot;  /* the material's texture variable (IdString32), such as a colour map's; 0 a material look */
    uint64_t own;
    uint64_t level[HF_EFFECT_LEVELS];
} HfEffectLook;

/* effect_state's answer. */
typedef struct HfEffectState
{
    uint32_t size;        /* sizeof(HfEffectState) */
    uint32_t level;       /* 0 none */
    float meter;
    float since_gain_s;   /* seconds since the meter last rose */
} HfEffectState;

#endif
