using Avalonia.Controls;
using Avalonia.Input;
using HellForge.Core;

namespace HellForge.Launcher;

// The Mods page's modpacks (ModSettings, Modpacks.cs): pick one to put its switches in use, make a new one with every mod
// on, or delete one. Only the switches belong to a pack; the order and each pack's options stay as they are.
public partial class MainWindow
{
    bool modpacksFilling; // the chooser being filled, not picked by the player
    string? modpackDeleteAsked; // the pack whose Delete was clicked once: a second click deletes it

    void InitializeModpacks()
    {
        ModpackChooser.SelectionChanged += (_, _) =>
        {
            if (modpacksFilling || paths.ModsFolder is not { } folder || ModpackChooser.SelectedItem is not string picked) return;
            ModpackAction(() => ModSettings.SwitchModpack(folder, picked == ModSettings.DefaultModpack ? null : picked), "switch the modpack");
        };
        NewModpackButton.Click += (_, _) =>
        {
            NewModpackPanel.IsVisible = true;
            NewModpackError.Text = "";
            NewModpackName.Text = "";
            NewModpackName.Focus();
        };
        CancelModpackButton.Click += (_, _) => NewModpackPanel.IsVisible = false;
        CreateModpackButton.Click += (_, _) => CreateModpack();
        NewModpackName.KeyDown += (_, e) =>
        {
            if (e.Key == Key.Enter) { CreateModpack(); e.Handled = true; }
            else if (e.Key == Key.Escape) { NewModpackPanel.IsVisible = false; e.Handled = true; }
        };
        DeleteModpackButton.Click += (_, _) =>
        {
            if (paths.ModsFolder is not { } folder || modsData?.Settings.Modpack is not { } active) return;
            if (modpackDeleteAsked != active)
            {
                modpackDeleteAsked = active;
                DeleteModpackButton.Content = $"Delete {active}? Click again";
                return;
            }
            modpackDeleteAsked = null;
            ModpackAction(() => ModSettings.DeleteModpack(folder, active), "delete the modpack");
        };
    }

    void CreateModpack()
    {
        if (paths.ModsFolder is not { } folder) return;
        var name = (NewModpackName.Text ?? "").Trim();
        if (!ModSettings.ValidModpackName(name))
        {
            NewModpackError.Text = "Use 1 to 32 letters, digits, spaces, - or _ (not \"Default\").";
            return;
        }
        if (modsData?.Modpacks.Contains(name, StringComparer.OrdinalIgnoreCase) == true)
        {
            NewModpackError.Text = $"There is a modpack named {name} already.";
            return;
        }
        NewModpackPanel.IsVisible = false;
        ModpackAction(() => ModSettings.CreateModpack(folder, name), "make the modpack");
    }

    void ModpackAction(Action action, string what)
    {
        try { action(); }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            Append($"Could not {what}: {error.Message}", Red);
        }
        RefreshMods();
    }

    // The chooser as the last load found the packs; a session in progress keeps the mods it started with, so no switching.
    void DrawModpacks(ModsPageData? data)
    {
        modpacksFilling = true;
        var active = data?.Settings.Modpack ?? ModSettings.DefaultModpack;
        List<string> names = [ModSettings.DefaultModpack, .. data?.Modpacks ?? []];
        if (!names.Contains(active)) names.Add(active);
        ModpackChooser.ItemsSource = names;
        ModpackChooser.SelectedItem = active;
        modpacksFilling = false;
        var usable = data is { Error: null } && paths.ModsFolder != null && !modsLocked;
        ModpackChooser.IsEnabled = NewModpackButton.IsEnabled = usable;
        DeleteModpackButton.IsEnabled = usable && data?.Settings.Modpack != null;
        if (modpackDeleteAsked != data?.Settings.Modpack) modpackDeleteAsked = null;
        DeleteModpackButton.Content = modpackDeleteAsked != null ? $"Delete {modpackDeleteAsked}? Click again" : "Delete";
        if (!usable) NewModpackPanel.IsVisible = false;
        ModpackHint.Text = modsLocked ? "Modpacks switch between sessions."
            : "A modpack keeps which mods are on. New ones start with every mod on; the order stays shared.";
    }
}
