/* HellForge mod SDK 34: the traits an item's card shows (docs/modding/traits.md). Include it beside hellforge_mod.h.
 * A trait is a text id: one of the game's (HF_ITEM_TRAIT_ below, the game's localised label) or the mod's own
 * (add_text(mod, "trait_cryo", "CRYO")). set_item_traits fills a loadout item's 5 slots (primaries, secondaries,
 * throwables; the armory card of a support weapon or backpack); set_stratagem_traits gives any stratagem card up to 4
 * lines of its own, in the mod's order, its flags untouched.
 *
 *     const uint32_t crawler[] = {HF_ITEM_TRAIT_SENTRY, hf->add_text(mod, "trait_mobile", "MOBILE"), HF_ITEM_TRAIT_MEDIUM_AP};
 *     hf->set_stratagem_traits(mod, crawlerType, crawler, 3); */
#ifndef HELLFORGE_TRAITS_H
#define HELLFORGE_TRAITS_H

#include <stdint.h>

#define HF_ITEM_TRAITS_MAX 5u      /* trait slots of a loadout item */
#define HF_STRATAGEM_TRAITS_MAX 4u /* lines of a stratagem card's ITEM TRAITS box (the game's limit) */

/* The game's trait labels (build 25480438). Each is a text id: the loadout items' trait slots hold them, and the
 * stratagem card's 25 tags are the first group, in the card's own order (game.dll's tag table). Any of them may go on
 * either kind of card. */
/* The stratagem tags (the card's order, StratagemInfoFlag bit in brackets where it is not the index) */
#define HF_ITEM_TRAIT_SUPPORT_WEAPON 0x072095C5u    /* SUPPORT WEAPON */
#define HF_ITEM_TRAIT_HELLPOD 0x939853EDu           /* HELLPOD */
#define HF_ITEM_TRAIT_EXPLOSIVE_TAG 0xC670BF20u     /* EXPLOSIVE (the stratagem tag; HF_ITEM_TRAIT_EXPLOSIVE the weapons') */
#define HF_ITEM_TRAIT_BACKPACK 0xEDE35B37u          /* BACKPACK */
#define HF_ITEM_TRAIT_ORBITAL 0x6D3E0435u           /* ORBITAL */
#define HF_ITEM_TRAIT_VEHICLE 0x6DCCE05Du           /* VEHICLE */
#define HF_ITEM_TRAIT_FIRE 0x1BA54742u              /* the fire tag (shows as the game's incendiary label) */
#define HF_ITEM_TRAIT_AERIAL 0xCDA36BBFu            /* AERIAL (Eagles) */
#define HF_ITEM_TRAIT_SENTRY 0x012061E1u            /* SENTRY (the turret tag) */
#define HF_ITEM_TRAIT_GAS 0x6F36222Eu               /* GAS (bit 10); the weapons' caustic trait is the same id */
#define HF_ITEM_TRAIT_HEAVY_AP 0x273E3C6Du          /* HEAVY ARMOR PENETRATING (bit 11) */
#define HF_ITEM_TRAIT_ANTI_TANK 0xD61881CEu         /* ANTI-TANK (bit 12) */
#define HF_ITEM_TRAIT_LIGHT_AP 0x11EF4115u          /* LIGHT ARMOR PENETRATING (bit 13) */
#define HF_ITEM_TRAIT_MEDIUM_AP 0xB7E2C047u         /* MEDIUM ARMOR PENETRATING (bit 14) */
#define HF_ITEM_TRAIT_STATIONARY_RELOAD 0x9C618BD4u /* STATIONARY RELOAD (bit 15) */
#define HF_ITEM_TRAIT_EXPENDABLE 0xF4A098E6u        /* EXPENDABLE (bit 16) */
#define HF_ITEM_TRAIT_HEAT 0xDF534D7Fu              /* HEAT (bit 17) */
#define HF_ITEM_TRAIT_BEAM 0x8161DB6Fu              /* BEAM (bit 18) */
#define HF_ITEM_TRAIT_CHARGEUP 0x88F76D27u          /* CHARGE-UP (bit 19) */
#define HF_ITEM_TRAIT_MELEE 0x289C44EDu             /* MELEE (bit 23) */
#define HF_ITEM_TRAIT_ONE_HANDED 0xF115B0A6u        /* ONE HANDED (bit 24) */
#define HF_ITEM_TRAIT_GUIDED 0xACF0CB5Fu            /* GUIDED (bit 25) */
#define HF_ITEM_TRAIT_STICKY 0x5E5A0B19u            /* STICKY (bit 26) */
#define HF_ITEM_TRAIT_ARC 0x3F748867u               /* ARC (bit 27) */
#define HF_ITEM_TRAIT_STUN 0x0332F4A6u              /* STUN (bit 28) */
/* The loadout items' own labels (no stratagem tag has them) */
#define HF_ITEM_TRAIT_EXPLOSIVE 0x4EFEA81Cu         /* EXPLOSIVE */
#define HF_ITEM_TRAIT_INCENDIARY 0x7D01A114u        /* INCENDIARY (0xA5F02451 is a second id with the same label) */
#define HF_ITEM_TRAIT_LIGHT_ANTI_TANK 0x24818136u   /* LIGHT ANTI-TANK */
#define HF_ITEM_TRAIT_STIMULATIVE 0xEB92DA81u       /* STIMULATIVE (healing) */
#define HF_ITEM_TRAIT_ROUNDS_RELOAD 0x597FB04Eu     /* ROUNDS RELOAD */
#define HF_ITEM_TRAIT_SUPPRESSED 0x7337DD82u        /* SUPPRESSED */
#define HF_ITEM_TRAIT_TEAM_WEAPON 0xCB2B1C44u       /* TEAM WEAPON */

#endif
