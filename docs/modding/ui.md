# Screens in the game's own UI

<!-- diataxis: how-to -->

A code mod can show screens made the way the game makes its own: XAML, parsed and drawn by the game's NoesisGUI. The
game's fonts, text styles, colours and themed controls are all there, so a mod's screen looks like part of the game.
The calls are SDK 19 (`ui_open`, `ui_set`, `ui_on`, `ui_close`); the types are in `hellforge_ui.h`, which
`hellforge_mod.h` includes.

For a screen drawn with HellForge's own drawing calls (a map, a graph), see [Screens over the game](overlay.md).

## Contents

- [A first screen](#a-first-screen)
- [Kinds of screen](#kinds-of-screen)
- [XAML](#xaml)
- [Changing a screen from code](#changing-a-screen-from-code)
- [Clicks and closing](#clicks-and-closing)
- [Texts for the game's own widgets](#texts-for-the-games-own-widgets)
- [Tabs on the game's tab bars](#tabs-on-the-games-tab-bars)
- [A category in Vehicle Customization](#a-category-in-vehicle-customization)
- [Settings in the MODS menu](#settings-in-the-mods-menu)
- [Limits](#limits)

## A first screen

Put the XAML in your mod's folder, say `ui/shop.xaml`, and name the elements your code talks to with `x:Name`:

```xml
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" Background="#B0000000">
  <Border HorizontalAlignment="Center" VerticalAlignment="Center" Width="560" Background="#F0101214"
          BorderThickness="0,4,0,0" Padding="36,22,36,26">
    <Border.BorderBrush><SolidColorBrush Color="{StaticResource BrandYellow}"/></Border.BorderBrush>
    <StackPanel>
      <TextBlock x:Name="Title" Style="{StaticResource Headerstyle}" Text="REQUISITION"/>
      <TextBlock x:Name="Body" Style="{StaticResource Body_Medium}" Text="Nothing bought yet."/>
      <Button x:Name="Buy" Content="BUY" Padding="24,8" Margin="0,18,0,0" HorizontalAlignment="Left"/>
    </StackPanel>
  </Border>
</Grid>
```

Then open it and listen to its button:

```c
static void Bought(HfMod* mod, int screen, const char* element, void* user)
{
    HF_API(mod)->ui_set(mod, screen, "Body", "Text", "Bought one.");
}

static void Closed(HfMod* mod, int screen, const char* element, void* user)
{
    HF_API(mod)->log(mod, "the shop closed");
}

/* from hellforge_mod_init, a game event or the game thread */
int shop = hf->ui_open(mod, "ui/shop.xaml", HF_UI_MENU);
hf->ui_on(mod, shop, "Buy", Bought, NULL);
hf->ui_on(mod, shop, NULL, Closed, NULL);
```

The manifest needs `"sdk": 19`. Every call may come from any thread: HellForge queues it and runs it on the game thread
at the next game update, the only thread that may touch the game's UI. Callbacks run on the game thread.

## Kinds of screen

| Kind | What it is |
|---|---|
| `HF_UI_MENU` | A menu, as the game's own: the cursor shows, the player's movement, camera and buttons are held, and Back (Esc) closes it. It opens once no game menu is open or opening, on the ship or in a mission. One mod menu is open at a time; another mod's `ui_open` of a menu is refused while one is up. |
| `HF_UI_OVERLAY` | Over the game, on the ship and in missions, with no cursor of its own: for timers, counters and small panels. It does not take clicks. It stays until the mod closes it. |

A menu takes the game's unused menu slot 40, which the game itself never opens, so no game screen is lost.

## XAML

The XAML is parsed by the game's NoesisGUI 3.2.5, which reads WPF's XAML. `ui_open` takes a file of the mod's folder
(a relative path, no `..`, 4 MB at most) or the XAML text itself, starting with `<`.

The game's global resources are in reach with `{StaticResource ...}`, among them:

| Resource | What it is |
|---|---|
| `Headerstyle` | the game's heading text style |
| `Body_Medium` | its body text style |
| `BrandYellow` | the game's yellow, a `Color` (use it in a `SolidColorBrush`) |

Stock controls (`Button`, `CheckBox`, `Slider`, `TextBox`, `ScrollViewer`...) already have the game's look. A screen has
no code behind it: leave out `x:Class`, and wire it up from C with the calls below.

When the XAML does not parse, `ui_open` still returns an id; the screen is refused when it would open, and the mod's
log says so. The game's Noesis log line says why.

## Changing a screen from code

`ui_set(mod, screen, element, property, value)` sets a property of a named element (`""` or NULL: the screen's root)
from text, the way XAML would read it:

```c
hf->ui_set(mod, shop, "Body", "Text", "3 left");
hf->ui_set(mod, shop, "Title", "Foreground", "#FFE900");
hf->ui_set(mod, shop, "Buy", "Visibility", "Collapsed");
hf->ui_set(mod, shop, "Buy", "IsEnabled", "False");
```

Any dependency property of the element or its base types works, through that property's own type converter. A name
that is not there, a property the element does not have, or text that is not a value of it is logged in the mod's log
(`ui(1): no element named "Nobody"`).

## Clicks and closing

`ui_on(mod, screen, element, callback, user)` calls `callback` when the named element is clicked. It takes anything
with a `Command` property: a `Button`, a `MenuItem`, a `Hyperlink`. Only a menu screen takes clicks.

`ui_on` with element NULL calls `callback` when the screen closes, with element `""`: after `ui_close`, or when the
player presses Back on a menu. A screen that closed is gone; open it again for a new one.

`ui_close(mod, screen)` closes a screen. A menu closes the way the game closes its own, at the end of that UI frame, so
it is safe from inside a click callback.

## Texts for the game's own widgets

Some of the game's widgets take a text id, not a string: a tab bar's labels, for one. `add_text` (SDK 22) gives the
mod's own string an id that the game's text lookup answers:

```c
#define HAS(hf, call) ((hf)->size >= offsetof(HfApi, call) + sizeof (hf)->call)
uint32_t label = HAS(hf, add_text) ? hf->add_text(mod, "support_tab", "SUPPORT") : 0;
```

- The key is unique within the mod. The same key gives the same id in every session.
- The id is served from the call on, on any thread. It returns 0 when refused, and the mod's log says why.

## Tabs on the game's tab bars

`add_menu_tab` (SDK 27) puts a label of the mod's on one of the game's own tab bars, after the game's labels. Clicks
and Q/E reach it as they reach the game's tabs. `add_armory_tab` (SDK 26) does the same on the Armory's four lists:

```c
static void OnSupport(HfMod* mod, uint32_t tab, int selected, void* user)
{
    /* selected 1: the player picked SUPPORT; 0: another tab, or the Armory closed */
}

uint32_t tab = HAS(hf, add_menu_tab) ? hf->add_menu_tab(mod, HF_MENU_TABS_ARMORY_WEAPONS, "SUPPORT", OnSupport, NULL) : 0;
```

| Bar | Where it shows | Vanilla labels |
| --- | --- | --- |
| `HF_MENU_TABS_ARMORY_CATEGORIES` | the Armory's overview, before a page opens | 5 |
| `HF_MENU_TABS_ARMORY_WEAPONS` | the weapons page: PRIMARY, SECONDARY, THROWABLE | 3 |
| `HF_MENU_TABS_ARMORY_ARMOUR` | the second category's page: helmet, armour, cape | 3 |
| `HF_MENU_TABS_ARMORY_COSMETICS` | the third category's page | 6 |
| `HF_MENU_TABS_LOADOUT` | the mission loadout screen | 1 or 2 |
| `HF_MENU_TABS_SPACE_STATION` | the Democracy Space Station | 3 |
| `HF_MENU_TABS_CUSTOMIZE` | the armory's customize screen of one weapon | 2 |
| `HF_MENU_TABS_INFO_TERMINAL` | the ship's Info Terminal | 1 |

- The Armory's four values are `HF_ARMORY_TABS_...`, so `add_armory_tab` takes them too.
- The Escape menu's and Change Bindings' bars are left out: the Vanilla Plus Megapack puts its MODS tabs there and
  expects the game's labels alone. The Store and Superstore bars are not reviewed yet.
- A bar holds 8 labels, so a bar takes as many mod tabs as it has room for. Mods get them in the order they ask.
- Picking a mod's tab does not run the game's select. The page keeps what it showed, and the mod changes it.
- The callback runs on the game thread, also from inside the game's own tab change. It is called with 0 when the
  screen closes. It may call the game's code.
- The label is the mod's own text, made as `add_text` makes one.

## A category in Vehicle Customization

`add_hangar_category` (SDK 41) shows a mod's cards in Ship Management > Vehicle Customization, as a 6th category
after the game's five. The game's cards, EQUIP and the equipped mark work on them; the mod keeps the pick:

```c
static const uint32_t kCards[] = {0x80FBE63Du, 0xBD04CA8Cu};
static const char* const kNames[] = {"DEFAULT", "VENUS"};

static void OnPick(HfMod* mod, uint32_t category, uint32_t card, void* user)
{
    /* the player equipped card: save it, and use it where the mod paints its units */
}

HfHangarCategory c = {sizeof c, "EMPLACEMENTS", 2, kCards, kNames, kCards[0], OnPick, NULL};
uint32_t category = HAS(hf, add_hangar_category) ? hf->add_hangar_category(mod, &c) : 0;
```

The tab itself comes from the mod's data spec: a [xaml edit](data.md#edit) of the Ship Management view
(`0x4849ACD71BDAEECB`) that adds a `TabItem` after `Frvs` in `VehiclesCategoryTabs`, with a header template such as
`ModuleRobotics`.

- One category in all, from `hellforge_mod_init`. `set_hangar_equipped` sets the card shown as equipped.
- The game keeps 12 unit skin picks, all in use, so it never saves or syncs the category's pick: the mod does.
- Cards have no thumbnail or 3D preview yet.
- `HF_HANGAR_CATEGORIES` is defined where the call exists.

## Settings in the MODS menu

`add_option` (SDK 30) adds a row to the escape menu's OPTIONS > MODS, under the mod's name. HellForge keeps the value
and saves it, so the mod only reads it and hears of changes:

```c
static void OnOption(HfMod* mod, const char* id, double value, void* user)
{
    /* the player applied a new value; game thread */
}

static const char* const kSpeeds[] = {"Slow", "Normal", "Fast"};
HfOption speed = {sizeof speed, HF_OPTION_CHOICE, "speed", "Pod speed", "How fast the pod falls.", 1};
speed.choice_count = 3, speed.choices = kSpeeds, speed.on_change = OnOption;
double value = 1;
if (HAS(hf, add_option) && hf->add_option(mod, &speed)) hf->get_option(mod, "speed", &value);
```

| Kind | Value | Also set |
| --- | --- | --- |
| `HF_OPTION_TOGGLE` | 0 off, 1 on | |
| `HF_OPTION_CHOICE` | the index into `choices`, from 0 | `choice_count` (2 to 16), `choices` |
| `HF_OPTION_SLIDER` | a number, snapped to `step` | `min`, `max`, `step` |

- HellForge draws the MODS tab itself, one button per mod. With the Vanilla Plus Megapack's Mod Options Menu on, the
  rows show in that menu instead, through the bundled HellForge Mod Options mod and Bingus Shared Loader.
- The id keys the saved value: a-z, 0-9, `_ . -`, up to 48. Keep it from one version to the next.
- Values are saved in `<logs>\sessions\mod-options\<mod id>.txt`. HellForge's value wins over the menu's own.
- `set_option` changes a value from code, and the menu's row follows. It does not call the mod's own `on_change`.
- A mod has up to 32 options. Labels take 64 characters, choices 48 and descriptions 400.

## Limits

- 64 screens across all mods per session, and 256 click handlers.
- One mod menu at a time.
- Screens are client only: they are not part of mod sync, and each player sees only their own.
- Images and other files referenced from a mod's XAML are not served yet.
- Only the reviewed game build (25480438): on another, screens are refused and `modloader.log` says why.
