/* Item icons (docs/modding/icons.md): the flat silhouette the HUD shows for a weapon, tool or backpack.
 * Included by refs.c, which checks the edits (RefIconCheck); hellforge_modloader.c reads the plan's lines (IconPlanLine).
 *
 * The icon is the u64 at +0x30 of the entity's encyclopedia record (component 76): a GUI material's name, which the HUD
 * resolves through the engine's material lookup (game.dll 0x181A89B, as 0x17EF954 does a stratagem's +0xB0) and draws.
 * A name with no material behind it crashes the game, so an edit of EncyclopediaEntry.icon (mappings ref "icon", checked
 * in refs.c) may only name an icon some mod's data spec builds (the plan's "icon" lines) or one a game entity shows. */

#define ICON_MAX 4096u
#define ICON_RECORD_ICON 0x30u

typedef struct BuiltIcon { uint64_t name; char mod[64]; } BuiltIcon;
static BuiltIcon* g_builtIcons;
static uint32_t g_builtIconCount, g_builtIconRoom;

/* icon <mod id> <name hash hex>: an icon (texture and GUI material) the mod's data spec builds. 1 when it is such a line. */
static int IconPlanLine(char** f, int n)
{
    if (n < 1 || strcmp(f[0], "icon")) return 0;
    char* end = NULL;
    uint64_t name = n == 3 && strlen(f[2]) == 16 ? strtoull(f[2], &end, 16) : 0;
    if (name == 0 || end == NULL || *end || g_builtIconCount == ICON_MAX) return 1;
    if (g_builtIconCount == g_builtIconRoom)
    {
        uint32_t room = g_builtIconRoom ? g_builtIconRoom * 2 : 64;
        BuiltIcon* grown = (BuiltIcon*)realloc(g_builtIcons, room * sizeof *grown);
        if (grown == NULL) return 1;
        g_builtIcons = grown;
        g_builtIconRoom = room;
    }
    BuiltIcon* b = &g_builtIcons[g_builtIconCount++];
    b->name = name;
    snprintf(b->mod, sizeof b->mod, "%s", f[1]);
    return 1;
}

/* Whether some mod's data spec builds this icon. */
static int IconBuilt(uint64_t name)
{
    for (uint32_t i = 0; i < g_builtIconCount; i++) if (g_builtIcons[i].name == name) return 1;
    return 0;
}

/* Whether a game entity's encyclopedia record shows this icon (read from the entity data as the game loaded it). */
static int IconOfGame(uint64_t name)
{
    const Component* c = LastComponentNamed("EncyclopediaEntryComponentData");
    if (c == NULL || g_region == 0 || c->stride < ICON_RECORD_ICON + 8) return 0;
    uintptr_t records = g_region + c->offset + 28 + c->recordOffset;
    for (uint32_t i = 0; i < c->records; i++)
    {
        uint64_t icon;
        if (SafeRead(records + (uintptr_t)i * c->stride + ICON_RECORD_ICON, &icon, 8)) return 0;
        if (icon == name) return 1;
    }
    return 0;
}
