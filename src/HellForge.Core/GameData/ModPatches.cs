using System.Buffers.Binary;
using System.Globalization;
using System.Security.Cryptography;
using System.Text;

namespace HellForge.Core.GameData;

// The patch files a mod ships (its data/ and its pack's chosen folders, not data a spec generates for it), read as the
// game resolves them: within one bundle the mod's highest patch that holds a resource is the one the game loads. A data
// spec edits another mod's resource from here (an edit's "source": {"mod": ...}), so the edit stands for that mod's
// version. Another mod's generated data is never read, so no spec's build waits on another's.
public sealed class ModPatches
{
    readonly record struct Patch(string Bundle, int Index, string Main);
    readonly record struct Entry(ulong MainOffset, uint MainSize, ulong StreamOffset, uint StreamSize, ulong GpuOffset, uint GpuSize);

    readonly List<Patch> patches;
    readonly Dictionary<string, Dictionary<ResourceKey, Entry>> tables = new(StringComparer.Ordinal);

    public string Id { get; }

    public ModPatches(ModManifest mod)
    {
        Id = mod.Id;
        patches = [.. Files(mod).Where(f => ModCatalog.PatchFile().Match(Path.GetFileName(f)) is { Success: true } m && m.Groups[2].Length == 0)
            .Select(f => new Patch(Path.GetFileName(f)[..16], int.Parse(ModCatalog.PatchFile().Match(Path.GetFileName(f)).Groups[1].Value, CultureInfo.InvariantCulture), f))
            .OrderBy(p => p.Bundle, StringComparer.Ordinal).ThenByDescending(p => p.Index)];
    }

    // Every patch file the mod ships, sidecars too: what its built edits depend on.
    public static IEnumerable<string> Files(ModManifest mod)
    {
        foreach (var file in mod.DataFiles) yield return Path.Combine(mod.Folder, "data", file);
        foreach (var folder in mod.PackFolders)
            foreach (var file in CosmeticPacks.PatchFiles(folder)) yield return Path.Combine(folder, file);
    }

    // The mod's patch files by name and SHA-256, for the cache key of a spec that edits them: a new version of the mod
    // builds the edit again from its new files.
    public static string Stamp(ModManifest mod)
    {
        var text = new StringBuilder();
        foreach (var file in Files(mod).OrderBy(Path.GetFileName, StringComparer.Ordinal).ThenBy(f => f, StringComparer.Ordinal))
        {
            using var stream = new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            text.Append(Path.GetFileName(file)).Append(' ').Append(Convert.ToHexStringLower(SHA256.HashData(stream))).Append('\n');
        }
        return Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(text.ToString())))[..16];
    }

    // The resource in each bundle the mod ships it in, as the game loads it from this mod: its highest patch there that
    // holds it. Empty when the mod does not ship it.
    public Dictionary<string, ResourceParts> Find(ResourceKey key)
    {
        var found = new Dictionary<string, ResourceParts>(StringComparer.Ordinal);
        foreach (var patch in patches)
        {
            if (found.ContainsKey(patch.Bundle) || !Table(patch.Main).TryGetValue(key, out var entry)) continue;
            found[patch.Bundle] = new(Slice(patch.Main, entry.MainOffset, entry.MainSize), Slice(patch.Main + ".gpu_resources", entry.GpuOffset, entry.GpuSize),
                Slice(patch.Main + ".stream", entry.StreamOffset, entry.StreamSize));
        }
        return found;
    }

    // A patch's resource table, read from its head alone: the parts are read only for what an edit names.
    Dictionary<ResourceKey, Entry> Table(string main)
    {
        if (tables.TryGetValue(main, out var table)) return table;
        using var file = File.OpenRead(main);
        var head = new byte[72];
        file.ReadExactly(head);
        if (BinaryPrimitives.ReadUInt32LittleEndian(head) != 0xF0000011) throw new InvalidDataException($"{Id}: {Path.GetFileName(main)} is not a resource archive.");
        var types = BinaryPrimitives.ReadInt32LittleEndian(head.AsSpan(4));
        var count = BinaryPrimitives.ReadInt32LittleEndian(head.AsSpan(8));
        var length = 72L + 32L * types + 80L * count;
        if (types < 0 || count < 0 || length > file.Length) throw new InvalidDataException($"{Id}: {Path.GetFileName(main)}'s resource table runs past its end.");
        var rows = new byte[length - 72];
        file.ReadExactly(rows);
        table = [];
        for (var i = 0; i < count; i++)
        {
            var e = rows.AsSpan(32 * types + 80 * i, 80);
            table[new(BinaryPrimitives.ReadUInt64LittleEndian(e[8..]), BinaryPrimitives.ReadUInt64LittleEndian(e))] = new(
                BinaryPrimitives.ReadUInt64LittleEndian(e[16..]), BinaryPrimitives.ReadUInt32LittleEndian(e[56..]),
                BinaryPrimitives.ReadUInt64LittleEndian(e[24..]), BinaryPrimitives.ReadUInt32LittleEndian(e[60..]),
                BinaryPrimitives.ReadUInt64LittleEndian(e[32..]), BinaryPrimitives.ReadUInt32LittleEndian(e[64..]));
        }
        return tables[main] = table;
    }

    byte[] Slice(string path, ulong offset, uint size)
    {
        if (size == 0) return [];
        if (!File.Exists(path)) throw new InvalidDataException($"{Id}: {Path.GetFileName(path)} is missing.");
        using var file = File.OpenRead(path);
        if (offset + size > (ulong)file.Length) throw new InvalidDataException($"{Id}: a resource runs past the end of {Path.GetFileName(path)}.");
        var data = new byte[size];
        file.Position = (long)offset;
        file.ReadExactly(data);
        return data;
    }
}
