using System.Security.Cryptography;
using System.Text;

namespace HellForge.Core.GameData;

// The layers every mod of a session adds to game array textures (a texture edit with "layers"), stacked in load order.
// Each mod's copy of such a texture holds the whole stack, so whichever mod's copy the game loads, every mod's shapes
// are there at the same numbers. A paint names a layer instead of a number ("<png>" for its own mod's, "<mod id>:<png>"
// for another's) and is built with the number the layer has on this machine: players' load orders may differ.
public sealed class LayerStack
{
    public sealed record Layer(string Mod, string Folder, string Png);

    readonly Dictionary<ulong, List<Layer>> byTexture;

    LayerStack(Dictionary<ulong, List<Layer>> layers, string self) => (byTexture, Self) = (layers, self);

    // The mod whose spec is built: its own layers are named without its id.
    public string Self { get; }

    public LayerStack Seen(string self) => new(byTexture, self);

    // The stack of mods in load order: each mod's layers for a texture once, in the order its spec lists them.
    public static LayerStack Of(IEnumerable<(string Mod, string Folder, DataSpec Spec)> mods)
    {
        var stack = new LayerStack([], "");
        foreach (var (mod, folder, spec) in mods)
            foreach (var edit in spec.Patches.SelectMany(p => p.Resources).OfType<EditResource>().Where(e => e.Layers != null))
            {
                if (!stack.byTexture.TryGetValue(edit.Id, out var layers)) stack.byTexture[edit.Id] = layers = [];
                foreach (var png in edit.Layers!)
                    if (!layers.Any(l => l.Mod == mod && l.Png == png)) layers.Add(new(mod, folder, png));
            }
        return stack;
    }

    // Whether a spec adds layers or names one, so its build depends on the other mods' layers.
    public static bool Uses(DataSpec spec) => spec.Patches.SelectMany(p => p.Resources)
        .Any(r => r is EditResource { Layers: not null } || r is CopyResource { Lut.Layers.Count: > 0 });

    public IReadOnlyList<Layer> For(ulong texture) => byTexture.TryGetValue(texture, out var layers) ? layers : [];

    // A layer name as Self's spec writes it: the texture it is added to and its place after the game's layers.
    public (ulong Texture, int Added) Find(string name)
    {
        var self = Self;
        var split = name.IndexOf(':', StringComparison.Ordinal);
        var (mod, png) = split > 0 ? (name[..split], name[(split + 1)..]) : (self, name);
        var found = byTexture.SelectMany(t => t.Value.Select((l, i) => (Texture: t.Key, Added: i, Layer: l))).Where(x => x.Layer.Mod == mod && x.Layer.Png == png).ToList();
        if (found.Count == 0) throw new InvalidDataException($"layer \"{name}\" is not added by {(mod.Length > 0 ? mod : "the mod")} in this session.");
        if (found.Select(f => f.Texture).Distinct().Count() > 1) throw new InvalidDataException($"layer \"{name}\" is added to more than one texture.");
        return (found[0].Texture, found[0].Added);
    }

    // For the data cache key: every stacked layer with its file's hash, so a change to any mod's layers rebuilds.
    public string Stamp()
    {
        var text = new StringBuilder();
        foreach (var (texture, layers) in byTexture.OrderBy(t => t.Key))
            foreach (var layer in layers)
            {
                using var stream = new FileStream(Path.Combine(layer.Folder, layer.Png), FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                text.Append("layer ").Append(texture.ToString("x16", System.Globalization.CultureInfo.InvariantCulture)).Append(' ').Append(layer.Mod).Append(' ')
                    .Append(layer.Png).Append(' ').Append(Convert.ToHexStringLower(SHA256.HashData(stream))).Append('\n');
            }
        return text.ToString();
    }
}
