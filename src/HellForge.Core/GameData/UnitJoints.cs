using System.Buffers.Binary;
using System.Globalization;

namespace HellForge.Core.GameData;

// A unit copy's joints moved to new rest positions (a data spec's "joints"): a vehicle's seats, wheels and interact
// points where a smaller body needs them. The unit's scene graph (header +0x34) holds the joint count, then each
// joint's local transform (64 bytes: a 3x3 rotation, the translation at +36, the scale at +48), each joint's world
// matrix (64 bytes, row vectors, translation at +48), the parents (u32, parent index << 16) and the name hashes.
// A moved joint keeps its rotation; its local translation is set so its world position is the one asked for, and
// every joint below it moves with it. Physics actors and interact zones that ride a moved joint move too. A joint given
// a new parent keeps its world rotation; the parent must come before it in the graph (a lower index), as the game
// works out the joints' poses in order. The clips animate a vehicle's bones (its suspension arms, the FRV's steering
// wheel) from the base's poses, so a wheel moved under them is drawn where the base had it: hung from the body instead,
// it stays where it was put.
public static class UnitJoints
{
    // A joint's name, or "#" and its 8-digit name hash for a joint the game names only by hash.
    public static uint Hash(string name) =>
        name.StartsWith('#') && name.Length == 9 && uint.TryParse(name.AsSpan(1), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var h)
            ? h : EngineHash.Id32(name);

    // Each joint named moved to its position in the unit's own space (metres, +X right, +Y forward, +Z up). Returns
    // how many joints moved, those below the named ones included.
    public static int Move(byte[] m, IReadOnlyDictionary<string, double[]> moves, IReadOnlyDictionary<string, string>? hang = null)
    {
        if (m.Length < 0x38) throw new InvalidDataException("the unit is too short to hold a scene graph.");
        var graph = (int)U32(m, 0x34);
        if (graph == 0) throw new InvalidDataException("the unit has no joints to move.");
        var n = (int)U32(m, graph);
        int local = graph + 16, world = local + 64 * n, parents = world + 64 * n, names = parents + 4 * n;
        if (n <= 0 || n > 4096 || names + 4 * n > m.Length) throw new InvalidDataException("the unit's scene graph is not laid out as reviewed.");
        var parent = new int[n];
        for (var i = 0; i < n; i++)
        {
            parent[i] = (int)(U32(m, parents + 4 * i) >> 16);
            if (parent[i] >= n) throw new InvalidDataException("a joint of the unit names a parent past its joints.");
        }
        var g = new Graph(n, local, world, parents, names, parent);
        foreach (var (name, to) in hang ?? new Dictionary<string, string>()) Reparent(m, g, name, to);
        var moved = new HashSet<int>();
        // Ancestors first: a move shifts every joint below it, so a child placed before its ancestor would shift again.
        int Depth(int j)
        {
            var depth = 0;
            for (var i = j; g.Parent[i] != i && depth < n; i = g.Parent[i]) depth++;
            return depth;
        }
        foreach (var (name, to) in moves.OrderBy(move => Depth(Find(m, g, move.Key)))) MoveOne(m, g, name, to, moved);
        return moved.Count;
    }

    readonly record struct Graph(int Count, int Local, int World, int Parents, int Names, int[] Parent);

    // The joint hung from another: its local rotation becomes its world rotation times the inverse of the new parent's,
    // so it keeps its world rotation (MoveOne then sets its translation).
    static void Reparent(byte[] m, Graph g, string name, string to)
    {
        var j = Find(m, g, name);
        var p = to == "root" ? 0 : Find(m, g, to);
        if (p >= j) throw new InvalidDataException($"{name}'s new parent {to} must come before it among the unit's joints.");
        var w = Rotation(m, g.World + 64 * j);
        var pr = Rotation(m, g.World + 64 * p);
        // local * parent = world (row vectors): each row of local is the row of world solved against the parent.
        for (var a = 0; a < 3; a++)
        {
            var row = Solve(pr, [w[a, 0], w[a, 1], w[a, 2]]) ?? throw new InvalidDataException($"{to} has a rotation that cannot be inverted.");
            for (var b = 0; b < 3; b++) W32(m, g.Local + 64 * j + 12 * a + 4 * b, row[b]);
        }
        var field = g.Parents + 4 * j;
        BinaryPrimitives.WriteUInt32LittleEndian(m.AsSpan(field), (uint)(p << 16) | (U32(m, field) & 0xFFFF));
        g.Parent[j] = p;
    }

    static double[,] Rotation(byte[] m, int at)
    {
        var r = new double[3, 3];
        for (var a = 0; a < 3; a++)
            for (var b = 0; b < 3; b++) r[a, b] = F32(m, at + 16 * a + 4 * b);
        return r;
    }

    static void MoveOne(byte[] m, Graph g, string name, double[] to, HashSet<int> moved)
    {
        var j = Find(m, g, name);
        var w = g.World + 64 * j;
        double[] delta = [to[0] - F32(m, w + 48), to[1] - F32(m, w + 52), to[2] - F32(m, w + 56)];
        // Local translation: (world - parent's world translation) times the inverse of the parent's world rotation.
        var p = g.World + 64 * g.Parent[j];
        var r = Rotation(m, p);
        double[] rel = [to[0] - F32(m, p + 48), to[1] - F32(m, p + 52), to[2] - F32(m, p + 56)];
        var t = Solve(r, rel) ?? throw new InvalidDataException($"the parent of {name} has a rotation that cannot be inverted.");
        for (var k = 0; k < 3; k++) W32(m, g.Local + 64 * j + 36 + 4 * k, t[k]);
        // The joint and everything below it: world translations shift by the same amount.
        foreach (var i in Enumerable.Range(0, g.Count).Where(i => Below(g.Parent, i, j)))
        {
            for (var k = 0; k < 3; k++) W32(m, g.World + 64 * i + 48 + 4 * k, F32(m, g.World + 64 * i + 48 + 4 * k) + delta[k]);
            moved.Add(i);
        }
    }

    static int Find(byte[] m, Graph g, string name)
    {
        var hash = Hash(name);
        var j = Enumerable.Range(0, g.Count).Where(i => U32(m, g.Names + 4 * i) == hash).DefaultIfEmpty(-1).First();
        if (j < 0) throw new InvalidDataException($"the unit has no joint named {name}.");
        if (j == 0) throw new InvalidDataException($"{name} is the unit's root; move the joints below it instead.");
        return j;
    }

    // Whether joint i is j or below it.
    static bool Below(int[] parent, int i, int j)
    {
        for (var at = i; ; at = parent[at])
        {
            if (at == j) return true;
            if (parent[at] == at || at == 0) return false;
        }
    }

    // x such that x * R = v (row vectors), by Cramer's rule on R's transpose.
    static double[]? Solve(double[,] r, double[] v)
    {
        double D(double[,] a) => a[0, 0] * (a[1, 1] * a[2, 2] - a[1, 2] * a[2, 1]) - a[0, 1] * (a[1, 0] * a[2, 2] - a[1, 2] * a[2, 0]) + a[0, 2] * (a[1, 0] * a[2, 1] - a[1, 1] * a[2, 0]);
        var t = new double[3, 3];
        for (var a = 0; a < 3; a++)
            for (var b = 0; b < 3; b++) t[a, b] = r[b, a];
        var det = D(t);
        if (Math.Abs(det) < 1e-9) return null;
        var x = new double[3];
        for (var c = 0; c < 3; c++)
        {
            var s = (double[,])t.Clone();
            for (var a = 0; a < 3; a++) s[a, c] = v[a];
            x[c] = D(s) / det;
        }
        return x;
    }

    static uint U32(byte[] m, int at) => BinaryPrimitives.ReadUInt32LittleEndian(m.AsSpan(at));
    static float F32(byte[] m, int at) => BinaryPrimitives.ReadSingleLittleEndian(m.AsSpan(at));
    static void W32(byte[] m, int at, double v) => BinaryPrimitives.WriteSingleLittleEndian(m.AsSpan(at), (float)v);
}

// A unit copy's own physics (a data spec's "physics"): a whole physics resource from the mod folder, or byte edits on
// the copy of its base's. Either way HellForge renames it to the copy (RenamePhysics), as it does the base's.
public sealed record PhysicsSpec(string? File, IReadOnlyList<BytesEdit> Bytes);

public static class PhysicsEdits
{
    // The bytes edited: each edit's expect must be what the base's physics holds there.
    public static byte[] Apply(byte[] main, IReadOnlyList<BytesEdit> edits)
    {
        var output = main.ToArray();
        foreach (var edit in edits)
        {
            if (edit.At < 0 || (long)edit.At + edit.Set.Length > main.Length)
                throw new InvalidDataException($"physics bytes at 0x{edit.At:X} (+{edit.Set.Length}) are outside its {main.Length} bytes.");
            if (!main.AsSpan(edit.At, edit.Expect.Length).SequenceEqual(edit.Expect))
                throw new InvalidDataException($"the physics bytes at 0x{edit.At:X} are {Convert.ToHexString(main, edit.At, edit.Expect.Length)}, not the expected {Convert.ToHexString(edit.Expect)} (another game build?).");
            edit.Set.CopyTo(output, edit.At);
        }
        return output;
    }

    // A physics resource from the mod folder: the physics type at +0 and, at +0x08, the unit it was made from, which
    // must be the copy's base (so the tool that wrote it started from the right vehicle).
    public static byte[] Check(byte[] file, ulong from, string path)
    {
        if (file.Length < 0x48 || BinaryPrimitives.ReadUInt64LittleEndian(file) != ResourceTypes.Physics)
            throw new InvalidDataException($"{path} is not a physics resource (its first 8 bytes are not the physics type).");
        var made = BinaryPrimitives.ReadUInt64LittleEndian(file.AsSpan(8));
        if (made != from) throw new InvalidDataException($"{path} was made from unit 0x{made:X16}, not the copy's base 0x{from:X16}.");
        return file;
    }
}
