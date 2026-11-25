/* HellForge mod SDK 21, 27, 28 and 30: effects on entities (play_effect, stop_effect), enemies near a point
 * (find_enemies), the weapon the armory shows (armory_weapon), menu tabs, line of sight, projectiles near a point,
 * sides, joint poses and mod options; docs/modding/world.md. It also holds the spawn director's enemy constants (SDK 17,
 * add_enemy). Included by hellforge_mod.h. */
#pragma once
#include <stdint.h>

/* The faction rosters of the enemy spawn director, for HfEnemy and HfEnemySwap (SDK 17). */
#define HF_ENEMY_TERMINIDS 1u
#define HF_ENEMY_AUTOMATONS 2u
#define HF_ENEMY_ILLUMINATE 4u

#define HF_ENEMY_LEVELS 10
/* HfEnemy flags */
#define HF_ENEMY_ALWAYS_LOADED 1u /* a mission loads its packages though no squad names its group (it spawns by other means) */

/* HfEffect.flags */
#define HF_EFFECT_SHARED 0x1u /* every machine of the lobby plays it too, on its own copy of the entity */

/* stop_effect's how */
#define HF_EFFECT_STOP 0u    /* it stops spawning particles; those already out live their life */
#define HF_EFFECT_DESTROY 1u /* it is gone at once */

/* A particle effect (play_effect): attached to an entity's unit, or placed in the world. Zero-initialize it and set
 * `size`. The particles must be loaded: from a package the mod loads (load_package, its data's session package) or one
 * the entity's own packages hold. */
typedef struct HfEffect
{
    uint32_t size;      /* sizeof(HfEffect) */
    uint32_t flags;     /* HF_EFFECT_... */
    uint64_t particles; /* the particles resource: resource_hash of its name ("content/fx/.../steam_vent") */
    /* The entity it follows (its unit carries it, and it goes when the unit goes), or 0: it stays at `position`. */
    uint32_t entity;
    uint32_t reserved;
    const char* node;   /* the unit's node it hangs from ("" or NULL: the root) */
    /* With an entity: its offset from the node, in the node's own axes (metres). Without: where it plays in the world. */
    float position[3];
} HfEffect;

/* find_enemies' answers, nearest first. */
typedef struct HfFoundEnemy
{
    uint32_t entity;
    uint32_t unit;       /* its unit's handle */
    uint64_t resource;   /* its type (HF_ENTITY_..., or the entity a mod added) */
    float position[3];   /* its unit's root in the world */
    float distance;      /* from the centre asked about (metres) */
    int32_t health;      /* its health pool now */
    int32_t max_health;  /* -1 when it does not read */
    uint32_t faction;    /* its faction mask (bit 0 is the Helldivers' side); 0 when it has none and its type says enemy */
    uint32_t flags;      /* HF_FOUND_... */
} HfFoundEnemy;

/* HfFoundEnemy.flags */
#define HF_FOUND_DOWNED 0x1u    /* downed, not dead */
#define HF_FOUND_AUTHORITY 0x2u /* this machine simulates it (is_authority) */

/* ---- SDK 28 --------------------------------------------------------------------------------------------------------- */

/* line_of_sight's answers */
#define HF_SIGHT_UNKNOWN 0u /* not known now (the mod's log says why): treat it as the caller sees fit */
#define HF_SIGHT_CLEAR 1u
#define HF_SIGHT_BLOCKED 2u

/* What a line_of_sight ray leaves out. Zero-initialize it and set `size`. */
typedef struct HfSight
{
    uint32_t size;      /* sizeof(HfSight) */
    uint32_t ignore[4]; /* units whose hits do not count, such as the turret's own and its wearer's (0: none) */
    float skip_from;    /* hits closer than this to `from` do not count (metres): the muzzle's own body */
    float skip_to;      /* nor hits closer than this in front of `to`: the target itself */
} HfSight;

/* The first hit that blocked a line_of_sight ray. */
typedef struct HfSightHit
{
    float distance;    /* from `from` (metres) */
    uint32_t unit;     /* the unit hit; 0 for the ground or a static object */
    float position[3]; /* where along the ray */
} HfSightHit;

/* find_projectiles' answers, nearest first. */
typedef struct HfFoundProjectile
{
    uint32_t slot;      /* its place in the projectile pool (0 .. 2047): set_projectile's handle while it flies */
    uint32_t type;      /* its ProjectileType (HF_PROJECTILE_..., or one add_projectile returned) */
    float position[3];
    float velocity[3];  /* metres a second */
    float distance;     /* from the centre asked about (metres) */
    uint32_t source;    /* the entity that fired it, 0 when none reads */
    uint64_t peer;      /* the peer its hits are credited to (a player's), 0 for none */
} HfFoundProjectile;

/* side_of's answers. A faction mask holds a bit per side; HF_FACTION_HELLDIVERS is the players'. As the game's AI
 * sorts what it sees: no mask is neutral, a mask sharing a bit is friendly, any other mask is hostile. */
#define HF_FACTION_HELLDIVERS 0x1u
#define HF_SIDE_UNKNOWN 0u /* not an entity of this world, or the factions do not read */
#define HF_SIDE_HOSTILE 1u
#define HF_SIDE_FRIENDLY 2u
#define HF_SIDE_NEUTRAL 3u

/* HfFind.sides: which of them find_entities lists */
#define HF_FIND_HOSTILE (1u << HF_SIDE_HOSTILE)
#define HF_FIND_FRIENDLY (1u << HF_SIDE_FRIENDLY)
#define HF_FIND_NEUTRAL (1u << HF_SIDE_NEUTRAL)
/* HfFind.flags */
#define HF_FIND_HURT 0x1u       /* only those below their max health */
#define HF_FIND_NOT_DOWNED 0x2u /* leave the downed out */

/* A find_entities query. Zero-initialize it and set `size`. Its answers are HfFoundEnemy records. */
typedef struct HfFind
{
    uint32_t size;      /* sizeof(HfFind) */
    uint32_t sides;     /* HF_FIND_...: which sides, seen from `toward` */
    uint32_t toward;    /* the faction mask they are on a side of (0: HF_FACTION_HELLDIVERS) */
    uint32_t flags;     /* HF_FIND_HURT, HF_FIND_NOT_DOWNED */
    float centre[3];
    float radius;       /* metres */
} HfFind;

/* A joint's pose in its parent's frame, as the engine keeps it: three axis rows, the offset from the parent, the
 * scale. pose_joint writes it, joint_pose reads it (HF_POSE_LOCAL). */
typedef struct HfJointPose
{
    float axes[9];
    float position[3];
    float scale[3];
    float reserved;    /* 0 */
} HfJointPose;

/* view_pose (SDK 48): where this machine's camera is and where it looks, as the last frame rendered it. World space,
 * metres, Z up. To put a world point p on an overlay screen (1080 units high, width wide): d = p - position; depth =
 * d . forward; x = width / 2 + (d . right) / depth * k, y = 540 - (d . up) / depth * k, with k = 540 / tan(fov / 2). */
typedef struct HfView
{
    uint32_t size;        /* sizeof(HfView) */
    float right[3];       /* the camera's right, unit length */
    float forward[3];     /* where it looks, unit length */
    float up[3];          /* its up, unit length */
    float position[3];    /* where it is */
    float fov;            /* its vertical field of view, radians (the game's default view: 0.96) */
} HfView;

/* joint_pose's spaces */
#define HF_POSE_WORLD 0u /* a 4x4 row-major matrix: the axis rows (with scale), then the position at [12..14] */
#define HF_POSE_LOCAL 1u /* an HfJointPose */

/* Called once a frame, after the game has animated every unit and just before the frame renders (on_render). */
typedef void (*HfRenderCallback)(HfMod* mod, void* user);
/* The id of a unit's node by name, as pose_joint and joint_pose take it (the engine's 32-bit name id). */
#define hf_node_id(mod, name) ((uint32_t)(HF_API(mod)->resource_hash(name) >> 32))

/* Shot rules (modify_shots, homing): what they take is this machine's player's own shots of one projectile type, fired
 * by the listed weapons (entity resources, ending in 0; NULL or none: any of the player's). Each shot is changed once, as
 * it leaves; other weapons firing the same projectile stay as they are. Without HF_SHOT_MULTIPLAYER only in a solo game
 * (another machine's copy of the shot flies as the game made it). */
#define HF_SHOT_MULTIPLAYER 0x1u
#define HF_HOMING_NO_RETARGET 0x2u /* homing: keep the first target */
#define HF_HOMING_FRIENDLY 0x4u    /* homing: home on the other players' Helldivers, not on enemies */
typedef struct HfShotRule
{
    uint32_t size;          /* sizeof(HfShotRule) */
    uint32_t projectile;    /* its ProjectileSettings type */
    const uint64_t* sources;
    float damage;           /* multipliers, 1 for unchanged: damage 0.01 .. 100, armor_penetration 0.1 .. 10, */
    float armor_penetration; /* speed 0.1 .. 10, gravity 0 .. 20, drag 0 .. 20 */
    float speed, gravity, drag;
    uint32_t flags;
} HfShotRule;
typedef struct HfHoming
{
    uint32_t size;          /* sizeof(HfHoming) */
    uint32_t projectile;
    const uint64_t* sources;
    float turn_rate;        /* degrees a second, 1 .. 1440 */
    float cone;             /* the half-angle it looks for targets in, 1 .. 180 degrees */
    float range;            /* metres, 1 .. 500 */
    float arm_distance;     /* metres flown before it steers, 0 .. 100 */
    float aim_height;       /* metres above the target's root, -5 .. 10 */
    uint32_t flags;         /* HF_SHOT_MULTIPLAYER, HF_HOMING_... */
} HfHoming;

/* armory_weapon's menu: the menu state the game shows. */
#define HF_MENU_ARMORY 2u     /* the Armory: its weapon tabs' grids, with CUSTOMIZE and EQUIP */
#define HF_MENU_CUSTOMIZE 33u /* the armory's customize screen of one weapon */

/* add_armory_tab's lists (SDK 26): the Armory's one tab bar shows one of them at a time. A bar holds 8 labels. */
#define HF_ARMORY_TABS_CATEGORIES 0u /* the overview's categories (5 vanilla labels): weapons first */
#define HF_ARMORY_TABS_WEAPONS 1u    /* the weapons page: PRIMARY, SECONDARY, THROWABLE */
#define HF_ARMORY_TABS_ARMOUR 2u     /* the second category's page (3 vanilla labels): helmet, armour, cape */
#define HF_ARMORY_TABS_COSMETICS 3u  /* the third category's page (6 vanilla labels) */
/* add_menu_tab's bars (SDK 27): the Armory's four lists above keep their values. Each bar holds 8 labels. */
#define HF_MENU_TABS_ARMORY_CATEGORIES HF_ARMORY_TABS_CATEGORIES
#define HF_MENU_TABS_ARMORY_WEAPONS HF_ARMORY_TABS_WEAPONS
#define HF_MENU_TABS_ARMORY_ARMOUR HF_ARMORY_TABS_ARMOUR
#define HF_MENU_TABS_ARMORY_COSMETICS HF_ARMORY_TABS_COSMETICS
#define HF_MENU_TABS_LOADOUT 4u       /* the mission loadout screen (1 or 2 vanilla labels) */
#define HF_MENU_TABS_SPACE_STATION 5u /* the Democracy Space Station (3 vanilla labels) */
#define HF_MENU_TABS_CUSTOMIZE 6u     /* the armory's customize screen of one weapon (2 vanilla labels) */
#define HF_MENU_TABS_INFO_TERMINAL 7u /* the ship's Info Terminal (1 vanilla label) */
/* SDK 45: the Armory's SKINS page. Its first tab adds a SKINS label to the categories; picking that opens a page whose
 * bar holds the mods' SKINS tabs alone (8 at most), over the weapons page's grid. add_armory_tab takes it too. */
#define HF_ARMORY_TABS_SKINS 8u
#define HF_MENU_TABS_ARMORY_SKINS HF_ARMORY_TABS_SKINS
typedef void (*HfArmoryTabCallback)(HfMod* mod, uint32_t tab, int selected, void* user);

/* A new ability (SDK 7, add_ability), moved here from hellforge_mod.h: a vanilla ability HellForge holds hold_seconds
 * before it runs, with sounds of its own (Wwise events heard near the entity; package holds their bank), under an id of
 * its own (2868 and up). An entity runs it where its data names an ability (HF_FIELD(Explosive, armed_ability),
 * HF_FIELD(AbilityWeapon, mode0_ability)). HF_ABILITY_HOLDS_LINKED_LAUNCH makes it a silo countdown for a copy of a
 * silo's targeting remote: firing it holds the linked missile's launch, then launches it at the laser's last mark, and
 * the missile follows the laser while it stays on. No base. Example: docs/modding/runtime.md "add_ability". */
#define HF_ABILITY_SILO_LAUNCH 2088u /* the MS-11 Solo Silo missile's armed ability: its flight, once launched */
#define HF_ABILITY_HOLDS_LINKED_LAUNCH 0x1u /* HfAbility.flags: a targeting remote's countdown before its missile launches */
typedef struct HfAbility
{
    uint32_t size;          /* sizeof(HfAbility) */
    uint32_t base;          /* the vanilla ability it runs after its hold (HF_ABILITY_...), or 0: done when the hold ends */
    float hold_seconds;     /* 0 .. 600 */
    uint32_t start_event;   /* posted when it is set off */
    uint32_t second_event;  /* posted every whole second of the hold after the first */
    uint32_t final_event;   /* posted when the hold ends and the base starts */
    uint64_t package;       /* the package holding the sounds' bank, loaded for the session (a catalogue package), or 0 */
    uint32_t flags;         /* HF_ABILITY_... flags (HellForge 2026-10-18; older mods leave it out) */
} HfAbility;

/* ---- SDK 30 --------------------------------------------------------------------------------------------------------- */

/* SDK 30: a mod's settings in the game's OPTIONS > MODS menu (add_option; docs/modding/ui.md). HellForge keeps each
 * value (saved per mod in the logs folder's parent, mod-options\<mod id>.txt) and is its source of truth: the mod reads
 * it with get_option and hears of the player's changes through on_change. The menu itself is the Vanilla Plus Megapack's
 * Mod Options Menu, reached through HellForge's bundled "HellForge Mod Options" Lua mod (it needs Bingus Shared Loader);
 * without them a mod's options keep their saved values and set_option still changes them. */
#define HF_OPTION_TOGGLE 1u /* value 0 off, 1 on */
#define HF_OPTION_CHOICE 2u /* value the 0-based index into choices */
#define HF_OPTION_SLIDER 3u /* value a number from min to max, snapped to step */
#define HF_OPTION_MAX_CHOICES 16u
#define HF_OPTION_MAX_PER_MOD 32u
/* The player applied a new value in the menu; on the game thread. A mod's own set_option does not call it. */
typedef void (*HfOptionCallback)(HfMod* mod, const char* id, double value, void* user);
typedef struct HfOption
{
    uint32_t size;           /* sizeof(HfOption) */
    uint32_t kind;           /* HF_OPTION_... */
    const char* id;          /* unique within the mod, a-z 0-9 _ . - (up to 48): it keys the saved value, keep it */
    const char* label;       /* the row's text (up to 64 characters, shown in upper case) */
    const char* description; /* the description box's text while the row is selected (up to 400), or NULL */
    double value;            /* the default, used until the player changes it */
    uint32_t choice_count;   /* a choice's 2 .. HF_OPTION_MAX_CHOICES names (up to 48 characters each) */
    const char* const* choices;
    double min, max, step;   /* a slider's: min < max, 0 < step <= max - min (step 0: 1) */
    uint32_t gap;            /* 1: space above the row */
    HfOptionCallback on_change; /* may be NULL (read get_option when it matters) */
    void* user;
} HfOption;
