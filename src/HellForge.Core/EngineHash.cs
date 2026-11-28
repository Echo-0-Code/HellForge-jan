using System.Text;

namespace HellForge.Core;

// The engine's IdString32 for names in its network configuration: the upper half of MurmurHash64A (seed 0).
public static class EngineHash
{
    public static uint Id32(string name) => (uint)(Murmur64A(Encoding.UTF8.GetBytes(name)) >> 32);

    public static ulong Murmur64A(ReadOnlySpan<byte> data)
    {
        const ulong m = 0xC6A4A7935BD1E995;
        var h = (ulong)data.Length * m;
        var whole = data.Length / 8 * 8;
        for (var i = 0; i < whole; i += 8)
        {
            var k = BitConverter.ToUInt64(data.Slice(i, 8)) * m;
            k ^= k >> 47;
            h = (h ^ (k * m)) * m;
        }
        if (whole != data.Length)
        {
            ulong tail = 0;
            for (var i = data.Length - 1; i >= whole; i--) tail = (tail << 8) | data[i];
            h = (h ^ tail) * m;
        }
        h ^= h >> 47;
        h *= m;
        return h ^ (h >> 47);
    }
}
