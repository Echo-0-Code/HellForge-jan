using System.Globalization;
using System.Text;
using System.Text.Json;

namespace HellForge.Core.GameData;

// A weapon paint declared as data ("paints" in hellforge-data.json): a copy of a vanilla paint whose colour tables are
// the spec's recoloured copies, which the loader adds as add_pattern would (ModLoaderPlan's paint lines), so a paint
// needs no code. Swaps: each of the base's material_luts and the copy that replaces it.
public sealed record DataPaint(string Name, uint Base, string? Title, string? Description, IReadOnlyList<double[]> Swatch,
    IReadOnlyList<(ulong From, ulong To)> Swaps, IReadOnlyList<uint> Weapons);

public static partial class DataSpecs
{
    // The vanilla paints whose material_luts HellForge knows, as add_pattern logs them: a paint names its base by these.
    static readonly Dictionary<string, (uint Id, ulong[] Luts)> PaintBases = new(StringComparer.Ordinal)
    {
        ["arctic_camo"] = (0x5D125795u, [0xB0BFC364B70A43D4, 0xCDA094196416D7F2, 0xC5305E01494E8621, 0x3E584C6C46903BE8,
            0x5F6C25316AFD1837, 0xEF578F5A202FF319, 0xB7A58DB8485A0574]),
    };

    const int MaxPaintSwaps = 16, MaxSwatch = 8, MaxPaintWeapons = 16;

    // The spec's paints and the session package of their colour tables (<first paint>/luts), or none.
    static (List<DataPaint> Paints, DataPatch? Patch) ReadPaints(JsonElement root)
    {
        if (!root.TryGetProperty("paints", out _)) return ([], null);
        var paints = new List<DataPaint>();
        var copies = new List<DataResource>();
        var i = 0;
        foreach (var e in Items(root, "paints", ""))
        {
            var at = $"paints[{i++}]";
            Fields(e, at, "name", "title", "description", "base", "luts", "swatch", "lut", "weapons");
            var name = Name(e, at);
            if (name.Length > 90) throw new InvalidDataException($"{at}: name is at most 90 characters.");
            if (paints.Any(p => p.Name == name)) throw new InvalidDataException($"{at}: another paint is named {name}.");
            var (id, luts) = PaintBase(e, at);
            var lut = ReadLut(e, at) ?? throw new InvalidDataException($"{at}: lut is required (the colour table edit of each of the base's LUTs).");
            var swaps = new List<(ulong, ulong)>();
            foreach (var from in luts)
            {
                var copy = $"{name}/{from.ToString("x16", CultureInfo.InvariantCulture)}";
                copies.Add(new CopyResource($"{at}.lut", ResourceTypes.Texture, from, copy, new Dictionary<int, ulong>(), [], null, null, lut));
                swaps.Add((from, EngineHash.Murmur64A(Encoding.UTF8.GetBytes(copy))));
            }
            paints.Add(new(name, id, OneLine(e, "title", at, 100), OneLine(e, "description", at, 300), Swatch(e, at), swaps, Weapons(e, at)));
        }
        if (paints.Count == 0) throw new InvalidDataException("paints is empty.");
        var package = paints[0].Name + "/luts";
        return (paints, new([], copies, new(package, EngineHash.Murmur64A(Encoding.UTF8.GetBytes(package)), true)));
    }

    // "base": a paint HellForge knows by name ("arctic_camo"), or a vanilla paint's id with "luts", its material_luts.
    static (uint Id, IReadOnlyList<ulong> Luts) PaintBase(JsonElement e, string at)
    {
        var text = Text(e, "base", at);
        if (PaintBases.TryGetValue(text, out var known))
            return !e.TryGetProperty("luts", out _) ? (known.Id, known.Luts) : throw new InvalidDataException($"{at}: luts is for a base given by id.");
        if (!Hash32().IsMatch(text))
            throw new InvalidDataException($"{at}: base is {string.Join(", ", PaintBases.Keys)}, or a vanilla paint's id (\"0x5D125795\") with its luts.");
        var luts = Items(e, "luts", at).Select(l => Id(l, $"{at}.luts")).Distinct().ToList();
        if (luts.Count is 0 or > MaxPaintSwaps) throw new InvalidDataException($"{at}: luts lists 1 to {MaxPaintSwaps} of the base's material_luts.");
        return (uint.Parse(text.AsSpan(2), NumberStyles.AllowHexSpecifier, CultureInfo.InvariantCulture), luts);
    }

    static string? OneLine(JsonElement e, string name, string at, int most)
    {
        if (!e.TryGetProperty(name, out _)) return null;
        var text = Text(e, name, at);
        return text.Length is > 0 && text.Length <= most && text != "-" && !text.Any(char.IsControl) ? text
            : throw new InvalidDataException($"{at}: {name} is one line of 1 to {most} characters.");
    }

    // The card's swatch: 1 to 8 colours, as a LUT cell takes them (linear RGB).
    static List<double[]> Swatch(JsonElement e, string at)
    {
        if (!e.TryGetProperty("swatch", out _)) return [];
        var colours = Items(e, "swatch", at).Select(c => Colour(c, $"{at}.swatch")).ToList();
        return colours.Count is > 0 and <= MaxSwatch ? colours : throw new InvalidDataException($"{at}: swatch has 1 to {MaxSwatch} colours.");
    }

    static List<uint> Weapons(JsonElement e, string at)
    {
        if (!e.TryGetProperty("weapons", out _)) return [];
        var weapons = Items(e, "weapons", at).Select(w => w.ValueKind == JsonValueKind.Number && w.TryGetUInt32(out var id) && id > 0 ? id
            : throw new InvalidDataException($"{at}: a weapon is a loadout id (a number).")).Distinct().ToList();
        return weapons.Count is > 0 and <= MaxPaintWeapons ? weapons : throw new InvalidDataException($"{at}: weapons lists 1 to {MaxPaintWeapons} loadout ids.");
    }
}
