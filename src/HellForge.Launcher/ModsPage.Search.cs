using Avalonia.Controls;
using Avalonia.Input;
using HellForge.Core;

namespace HellForge.Launcher;

// The Mods page's search box: it sits above the list's scroller, so it stays in view. Typing hides the rows that do not
// match instead of drawing the page again, which keeps the list where it is and stays quick with many mods.
public partial class MainWindow
{
    // every drawn mod row, and each group's count with its rows, for the filter
    readonly List<(ModManifest Mod, Border Row)> modRowsDrawn = [];
    readonly List<(TextBlock Count, List<(ModManifest Mod, Border Row)> Rows, int Total)> modGroupCounts = [];
    string[] modSearchTerms = [];

    bool ModFilterActive => modSearchTerms.Length > 0;

    void InitializeModSearch()
    {
        ModSearchBox.TextChanged += (_, _) =>
        {
            modSearchTerms = (ModSearchBox.Text ?? "").Split(' ', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            ApplyModFilter();
        };
        ModSearchBox.KeyDown += (_, e) =>
        {
            if (e.Key != Key.Escape) return;
            ModSearchBox.Text = "";
            e.Handled = true;
        };
        KeyDown += (_, e) =>
        {
            if (!ModsPage.IsVisible || e.Key != Key.F || !e.KeyModifiers.HasFlag(KeyModifiers.Control)) return;
            ModSearchBox.Focus();
            ModSearchBox.SelectAll();
            e.Handled = true;
        };
    }

    // Every word must appear in the mod's name, id, file or one of its tags (mod manifests carry no author).
    bool ModMatches(ModManifest mod)
    {
        if (!ModFilterActive) return true;
        var tags = string.Join(' ', Tags(mod, modsData?.Builds.GetValueOrDefault(mod.Id)).Select(t => t.Label));
        var text = $"{mod.Name} {mod.Id} {mod.Archive ?? Path.GetFileName(mod.Folder)} {tags}";
        return modSearchTerms.All(term => text.Contains(term, StringComparison.OrdinalIgnoreCase));
    }

    void ApplyModFilter()
    {
        foreach (var (mod, row) in modRowsDrawn) row.IsVisible = ModMatches(mod);
        foreach (var (count, rows, total) in modGroupCounts)
            count.Text = ModFilterActive ? $"{rows.Count(r => r.Row.IsVisible)} of {total}" : total.ToString();
    }
}
