using System.Text.Json;
using System.Text.Json.Nodes;

namespace HellForge.Core;

// Modpacks: named sets of mod switches, for going between, say, a testing and a gaming setup. hellforge-mods.json always
// holds the switches in use, so every reader of it (sessions, the CLI, kept versions) needs nothing new. Each named pack
// has its switches in hellforge-mods-<name>.json; while one is on, the default pack's wait in hellforge-mods.json's
// "defaultDisabled". Only the switches change with the pack: the order, pack options and trust stay.
// A pack that is not in use keeps the mods installed when it was left ("known"; "defaultKnown" for the default pack),
// so a mod installed since starts off in it: a new testing mod is on only in the pack it was installed under.
public static partial class ModSettings
{
    public const string ModpackPrefix = "hellforge-mods-";
    public const string DefaultModpack = "Default";

    static void ReadModpack(JsonElement root, State state)
    {
        if (!root.TryGetProperty("modpack", out var pack) || pack.ValueKind != JsonValueKind.String || !ValidModpackName(pack.GetString()!)) return;
        state.Modpack = pack.GetString();
        state.DefaultDisabled = new HashSet<string>(StringComparer.Ordinal);
        AddTexts(root, "defaultDisabled", state.DefaultDisabled);
        if (root.TryGetProperty("defaultKnown", out var known) && known.ValueKind == JsonValueKind.Array)
        {
            state.DefaultKnown = new HashSet<string>(StringComparer.Ordinal);
            AddTexts(root, "defaultKnown", state.DefaultKnown);
        }
    }

    // The pack in use is saved with every change, so its own file always shows its switches.
    static void WriteModpack(string modsFolder, JsonObject root, State state)
    {
        if (state.Modpack == null) return;
        WriteModpack(modsFolder, state.Modpack, state.Disabled, null);
        root["modpack"] = state.Modpack;
        root["defaultDisabled"] = new JsonArray([.. (state.DefaultDisabled ?? []).Order(StringComparer.Ordinal).Select(id => (JsonNode?)id)]);
        if (state.DefaultKnown != null) root["defaultKnown"] = new JsonArray([.. state.DefaultKnown.Order(StringComparer.Ordinal).Select(id => (JsonNode?)id)]);
    }

    // A name that is a plain file name on every system, and not the default pack's.
    public static bool ValidModpackName(string name) =>
        name.Length is >= 1 and <= 32 && name.Trim() == name && !name.Equals(DefaultModpack, StringComparison.OrdinalIgnoreCase) &&
        name.All(c => char.IsAsciiLetterOrDigit(c) || c is ' ' or '-' or '_');

    public static string ModpackFile(string modsFolder, string name) => Path.Combine(modsFolder, ModpackPrefix + name + ".json");

    public static bool IsModpackFile(string fileName) =>
        fileName.StartsWith(ModpackPrefix, StringComparison.OrdinalIgnoreCase) && fileName.EndsWith(".json", StringComparison.OrdinalIgnoreCase);

    // The named packs (the default one is not listed), by name.
    public static List<string> Modpacks(string modsFolder)
    {
        if (!Directory.Exists(modsFolder)) return [];
        return Directory.EnumerateFiles(modsFolder, ModpackPrefix + "*.json").Select(Path.GetFileName).OfType<string>()
            .Select(f => f[ModpackPrefix.Length..^".json".Length]).Where(ValidModpackName).Order(StringComparer.OrdinalIgnoreCase).ToList();
    }

    // The pack in use, or null for the default one.
    public static string? ActiveModpack(string modsFolder) => Read(modsFolder).Modpack;

    // A new pack with every mod on, put in use at once (the switches before it are kept in their own pack).
    public static void CreateModpack(string modsFolder, string name)
    {
        if (!ValidModpackName(name)) throw new InvalidDataException("A modpack name has 1 to 32 letters, digits, spaces, - or _, and is not \"Default\".");
        if (Modpacks(modsFolder).Contains(name, StringComparer.OrdinalIgnoreCase)) throw new InvalidDataException($"There is a modpack named {name} already.");
        Directory.CreateDirectory(modsFolder);
        WriteModpack(modsFolder, name, [], null);
        SwitchModpack(modsFolder, name);
    }

    // The mods in the folder now, by id (folders and archives; one that fails to load is still listed by nothing).
    static HashSet<string> InstalledMods(string modsFolder) =>
        new(ModCatalog.Load(modsFolder).Mods.Select(m => m.Id), StringComparer.Ordinal);

    // Puts a pack's switches in use (null: the default pack), after keeping the current ones in their own pack. Mods
    // installed since the target pack was left start off in it.
    public static void SwitchModpack(string modsFolder, string? name)
    {
        var state = Read(modsFolder);
        if (string.Equals(name, state.Modpack, StringComparison.OrdinalIgnoreCase)) return;
        HashSet<string> target;
        HashSet<string>? known;
        if (name == null) { target = state.DefaultDisabled ?? []; known = state.DefaultKnown; }
        else
        {
            if (!File.Exists(ModpackFile(modsFolder, name))) throw new FileNotFoundException($"There is no modpack named {name}.");
            (target, known) = ReadModpack(ModpackFile(modsFolder, name));
        }
        var installed = InstalledMods(modsFolder);
        if (known is { } seen) target.UnionWith(installed.Where(id => !seen.Contains(id)));
        if (state.Modpack != null) WriteModpack(modsFolder, state.Modpack, state.Disabled, installed);
        Change(modsFolder, s =>
        {
            if (s.Modpack == null) { s.DefaultDisabled = [.. s.Disabled]; s.DefaultKnown = [.. installed]; }
            s.Disabled.Clear();
            s.Disabled.UnionWith(target);
            s.Modpack = name;
            if (name == null) { s.DefaultDisabled = null; s.DefaultKnown = null; }
        });
    }

    // Deletes a named pack; when it is in use, the default pack comes back first.
    public static void DeleteModpack(string modsFolder, string name)
    {
        if (!ValidModpackName(name)) throw new InvalidDataException($"\"{name}\" is not a modpack name.");
        if (string.Equals(ActiveModpack(modsFolder), name, StringComparison.OrdinalIgnoreCase)) SwitchModpack(modsFolder, null);
        File.Delete(ModpackFile(modsFolder, name));
    }

    // A pack that cannot be read is refused, not taken as "every mod on": a broken gaming pack would otherwise switch
    // every testing mod on with no word.
    static (HashSet<string> Disabled, HashSet<string>? Known) ReadModpack(string path)
    {
        var disabled = new HashSet<string>(StringComparer.Ordinal);
        HashSet<string>? known = null;
        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            if (document.RootElement.ValueKind != JsonValueKind.Object || !document.RootElement.TryGetProperty("disabled", out var list) ||
                list.ValueKind != JsonValueKind.Array)
                throw new InvalidDataException($"The modpack file {Path.GetFileName(path)} has no \"disabled\" list; fix or delete it.");
            AddTexts(document.RootElement, "disabled", disabled);
            if (document.RootElement.TryGetProperty("known", out var mods) && mods.ValueKind == JsonValueKind.Array)
            {
                known = new HashSet<string>(StringComparer.Ordinal);
                AddTexts(document.RootElement, "known", known);
            }
        }
        catch (JsonException error)
        {
            throw new InvalidDataException($"The modpack file {Path.GetFileName(path)} is not valid JSON ({error.Message}); fix or delete it.", error);
        }
        return (disabled, known);
    }

    // known: the mods installed as the pack is left (null while it is in use: what is installed then is on in it).
    static void WriteModpack(string modsFolder, string name, IEnumerable<string> disabled, IEnumerable<string>? known)
    {
        var path = ModpackFile(modsFolder, name);
        var root = new JsonObject
        {
            ["format"] = 1,
            ["disabled"] = new JsonArray([.. disabled.Order(StringComparer.Ordinal).Select(id => (JsonNode?)id)])
        };
        if (known != null) root["known"] = new JsonArray([.. known.Order(StringComparer.Ordinal).Select(id => (JsonNode?)id)]);
        File.WriteAllText(path + ".tmp", root.ToJsonString(new JsonSerializerOptions { WriteIndented = true }) + "\n");
        File.Move(path + ".tmp", path, overwrite: true);
    }
}
