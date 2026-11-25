using System.Text.Json;
using System.Text.Json.Nodes;

namespace HellForge.Core;

// Modpacks: named sets of mod switches, for going between, say, a testing and a gaming setup. hellforge-mods.json always
// holds the switches in use, so every reader of it (sessions, the CLI, kept versions) needs nothing new. Each named pack
// has its switches in hellforge-mods-<name>.json; while one is on, the default pack's wait in hellforge-mods.json's
// "defaultDisabled". Only the switches change with the pack: the order, pack options and trust stay.
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
    }

    // The pack in use is saved with every change, so its own file always shows its switches.
    static void WriteModpack(string modsFolder, JsonObject root, State state)
    {
        if (state.Modpack == null) return;
        WriteModpack(modsFolder, state.Modpack, state.Disabled);
        root["modpack"] = state.Modpack;
        root["defaultDisabled"] = new JsonArray([.. (state.DefaultDisabled ?? []).Order(StringComparer.Ordinal).Select(id => (JsonNode?)id)]);
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
        WriteModpack(modsFolder, name, []);
        SwitchModpack(modsFolder, name);
    }

    // Puts a pack's switches in use (null: the default pack), after keeping the current ones in their own pack.
    public static void SwitchModpack(string modsFolder, string? name)
    {
        var state = Read(modsFolder);
        if (string.Equals(name, state.Modpack, StringComparison.OrdinalIgnoreCase)) return;
        HashSet<string> target;
        if (name == null) target = state.DefaultDisabled ?? [];
        else
        {
            if (!File.Exists(ModpackFile(modsFolder, name))) throw new FileNotFoundException($"There is no modpack named {name}.");
            target = ReadModpack(ModpackFile(modsFolder, name));
        }
        if (state.Modpack != null) WriteModpack(modsFolder, state.Modpack, state.Disabled);
        Change(modsFolder, s =>
        {
            if (s.Modpack == null) s.DefaultDisabled = [.. s.Disabled];
            s.Disabled.Clear();
            s.Disabled.UnionWith(target);
            s.Modpack = name;
            if (name == null) s.DefaultDisabled = null;
        });
    }

    // Deletes a named pack; when it is in use, the default pack comes back first.
    public static void DeleteModpack(string modsFolder, string name)
    {
        if (string.Equals(ActiveModpack(modsFolder), name, StringComparison.OrdinalIgnoreCase)) SwitchModpack(modsFolder, null);
        File.Delete(ModpackFile(modsFolder, name));
    }

    static HashSet<string> ReadModpack(string path)
    {
        var disabled = new HashSet<string>(StringComparer.Ordinal);
        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            if (document.RootElement.ValueKind == JsonValueKind.Object) AddTexts(document.RootElement, "disabled", disabled);
        }
        catch (Exception error) when (error is JsonException or IOException) { }
        return disabled;
    }

    static void WriteModpack(string modsFolder, string name, IEnumerable<string> disabled)
    {
        var path = ModpackFile(modsFolder, name);
        var root = new JsonObject
        {
            ["format"] = 1,
            ["disabled"] = new JsonArray([.. disabled.Order(StringComparer.Ordinal).Select(id => (JsonNode?)id)])
        };
        File.WriteAllText(path + ".tmp", root.ToJsonString(new JsonSerializerOptions { WriteIndented = true }) + "\n");
        File.Move(path + ".tmp", path, overwrite: true);
    }
}
