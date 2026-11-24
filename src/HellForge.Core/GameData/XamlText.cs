using System.Buffers.Binary;
using System.Text;

namespace HellForge.Core.GameData;

// A game XAML view with text put in at named places (a xaml edit's "insert"). The resource is a u32 text length, 12
// zero bytes and the UTF-8 text. Each anchor must occur exactly once, so a game update that moves or repeats it refuses
// the edit instead of putting the text somewhere else. The game's views end lines with CRLF, so the anchor and the text
// take the view's line ending: a spec written with plain newlines still matches.
public static class XamlText
{
    const int Header = 16;

    public static byte[] Insert(byte[] resource, IReadOnlyList<XamlInsert> inserts)
    {
        if (resource.Length < Header || BinaryPrimitives.ReadUInt32LittleEndian(resource) != resource.Length - Header)
            throw new InvalidDataException("the resource is not a XAML view (a text length and 12 bytes, then the text).");
        var text = Encoding.UTF8.GetString(resource, Header, resource.Length - Header);
        var newline = text.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
        foreach (var insert in inserts)
        {
            var after = Lines(insert.After, newline);
            var at = text.IndexOf(after, StringComparison.Ordinal);
            if (at < 0) throw new InvalidDataException($"the view has no \"{Short(insert.After)}\".");
            if (text.IndexOf(after, at + 1, StringComparison.Ordinal) >= 0)
                throw new InvalidDataException($"the view has \"{Short(insert.After)}\" more than once.");
            text = text.Insert(at + after.Length, Lines(insert.Text, newline));
        }
        var body = Encoding.UTF8.GetBytes(text);
        var output = new byte[Header + body.Length];
        BinaryPrimitives.WriteUInt32LittleEndian(output, (uint)body.Length);
        resource.AsSpan(4, Header - 4).CopyTo(output.AsSpan(4));
        body.CopyTo(output, Header);
        return output;
    }

    static string Lines(string text, string newline) => text.Replace("\r\n", "\n", StringComparison.Ordinal).Replace("\n", newline, StringComparison.Ordinal);

    static string Short(string text) => text.Length <= 60 ? text : text[..60] + "...";
}
