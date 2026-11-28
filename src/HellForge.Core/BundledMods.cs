using HellForge.Core.GameData;

namespace HellForge.Core;

// Mods that ship with HellForge, built (the in-game editor, the Hell Forge difficulty, the Vanilla Plus Megapack):
// <installation>/app/bundled-mods/<folder> (scripts/Publish-HellForge.ps1), copied into <installation>/mods/<folder> when
// the launcher starts, when its setup has copied the game, and before each session, when one is missing or differs, so
// a new HellForge brings its new version. Only the shipped files are replaced, and the ones an older version shipped
// and this one does not are removed (.hellforge-bundled.txt): what a mod keeps in its folder (the in-game editor's
// settings, presets and mod-stats.json) stays. Switching one off on the Mods page keeps it off.
// A copy of a bundled mod that the player put in mods themselves (the same mod id: in any folder, or a zip) of the same
// version or newer is theirs to keep: it is left as it is and the bundled one is not installed beside it. An older one
// is moved to <installation>/mods-retired, keeping what the player saved in it, so the bundled one is the only copy.
// The in-game editor's catalogue, icons and fonts are made here, the icons and fonts from the installation's own game
// copy: they are the player's game files and are never part of what HellForge ships.
public static class BundledMods
{
    public const string FolderName = "bundled-mods";
    public const string EditorFolder = "hellforge-editor";
    public const string RetiredFolder = "mods-retired";
    const string ListName = ".hellforge-bundled.txt"; /* the files the copy in mods got from the app, one per line */
    // What a bundled mod keeps for its player (relative paths; a folder ends in '/'): carried into the bundled copy from
    // one that is retired.
    static readonly Dictionary<string, string[]> Kept = new(StringComparer.OrdinalIgnoreCase)
    {
        [EditorFolder] = ["editor-settings.json", "mod-stats.json", "presets/", "translations/"],
    };

    // Mods an earlier HellForge bundled and this one does not (their folder in mods, their id): the copy HellForge put
    // there (it has the shipped-files list and no sources) is moved to mods-retired. Vanilla Plus's three fixes are the
    // Vanilla Plus Megapack's own components now, which it keeps off while Vanilla Plus is on.
    static readonly (string Folder, string Id)[] Withdrawn = [("vanilla-plus", "skyeshade.vanilla-plus")];

    public static string Folder(InstallLayout install) => Path.Combine(install.App, FolderName);

    // A mod's id and version from its manifest (a folder's, or the one a zip holds at its root or one folder down), or null.
    sealed record Identity(string Id, Version Version, string Text);

    static Identity? IdentityOf(string json)
    {
        try
        {
            using var document = System.Text.Json.JsonDocument.Parse(json);
            var root = document.RootElement;
            if (root.ValueKind != System.Text.Json.JsonValueKind.Object || !root.TryGetProperty("id", out var id) || id.GetString() is not { Length: > 0 } text) return null;
            var version = root.TryGetProperty("version", out var v) && v.GetString() is { } raw ? raw : "0";
            return new(text, ParseVersion(version), version);
        }
        catch (Exception error) when (error is System.Text.Json.JsonException or InvalidOperationException) { return null; }
    }

    static Version ParseVersion(string text)
    {
        var parts = text.Split('-', '+')[0].Split('.').Select(p => int.TryParse(p, out var n) && n >= 0 ? n : 0).Concat([0, 0, 0]).Take(4).ToArray();
        return new Version(parts[0], parts[1], parts[2], parts[3]);
    }

    static Identity? FolderIdentity(string folder) =>
        ModCatalog.ManifestIn(folder) is { } manifest ? IdentityOf(File.ReadAllText(manifest)) : null;

    static Identity? ZipIdentity(string archive)
    {
        try
        {
            using var zip = System.IO.Compression.ZipFile.OpenRead(archive);
            var entry = zip.Entries.Where(e => e.Name == ModCatalog.ManifestName && e.FullName.Replace('\\', '/').Count(c => c == '/') <= 1)
                .OrderBy(e => e.FullName.Length).FirstOrDefault();
            if (entry == null) return null;
            using var reader = new StreamReader(entry.Open());
            return IdentityOf(reader.ReadToEnd());
        }
        catch (Exception error) when (error is InvalidDataException or IOException or UnauthorizedAccessException) { return null; }
    }

    // The problems (a bundled mod that could not be put in place); the session still runs without it.
    public static List<string> Sync(InstallLayout install, Action<string>? log = null)
    {
        var problems = new List<string>();
        var source = Folder(install);
        if (!Directory.Exists(source)) return problems;
        foreach (var (folder, id) in Withdrawn)
        {
            try { RetireWithdrawn(install, folder, id, log); }
            catch (Exception error) when (error is IOException or UnauthorizedAccessException)
            {
                problems.Add($"{folder} (bundled with an earlier HellForge) could not be moved to {RetiredFolder}: {error.Message}");
            }
        }
        foreach (var shipped in Directory.GetDirectories(source).Order(StringComparer.OrdinalIgnoreCase))
        {
            var name = Path.GetFileName(shipped);
            var target = Path.Combine(install.Mods, name);
            try { SyncOne(install, shipped, name, target, log); }
            catch (Exception error) when (error is IOException or UnauthorizedAccessException or InvalidDataException)
            {
                problems.Add($"{name} (bundled with HellForge) could not be installed: {error.Message}");
            }
        }
        return problems;
    }

    // mods/<folder> retired when it is the copy an earlier HellForge bundled there: its id, the shipped-files list, no
    // sources, and this HellForge ships nothing to that folder. A copy the player put there themselves stays.
    static void RetireWithdrawn(InstallLayout install, string folder, string id, Action<string>? log)
    {
        var path = Path.Combine(install.Mods, folder);
        if (Directory.Exists(Path.Combine(Folder(install), folder)) || !File.Exists(Path.Combine(path, ListName)) ||
            Directory.Exists(Path.Combine(path, "src")) || FolderIdentity(path) is not { } copy || copy.Id != id) return;
        var retired = Directory.CreateDirectory(Path.Combine(install.Root, RetiredFolder)).FullName;
        var destination = Path.Combine(retired, folder + "-" + copy.Text);
        for (var n = 2; File.Exists(destination) || Directory.Exists(destination); n++) destination = Path.Combine(retired, $"{folder}-{copy.Text}-{n}");
        Directory.Move(path, destination);
        log?.Invoke($"Bundled mod {folder}: no longer bundled with this HellForge version, so its copy (version {copy.Text}) was moved to {RetiredFolder}/{Path.GetFileName(destination)}.");
    }

    // One bundled mod into mods/<name>, unless a copy of the player's own keeps its place.
    static void SyncOne(InstallLayout install, string shipped, string name, string target, Action<string>? log)
    {
        var bundled = FolderIdentity(shipped);
        if (bundled != null && !Settle(install, name, target, bundled, log)) return;
        if (Directory.Exists(Path.Combine(target, "src")))
        {
            log?.Invoke($"Bundled mod {name}: mods/{name} has sources of its own, so HellForge's built copy is not installed over it.");
            return;
        }
        var files = Directory.GetFiles(shipped, "*", SearchOption.AllDirectories).Select(f => Path.GetRelativePath(shipped, f).Replace('\\', '/')).ToList();
        var changed = CopyShipped(shipped, target, files);
        // the files an earlier version shipped and this one does not
        var list = Path.Combine(target, ListName);
        var before = File.Exists(list) ? File.ReadAllLines(list).Where(l => l.Length > 0).ToList() : [];
        if (DeleteStale(target, before.Except(files, StringComparer.OrdinalIgnoreCase))) changed = true;
        if (!before.SequenceEqual(files, StringComparer.OrdinalIgnoreCase))
            File.WriteAllLines(list, files);
        if (changed) log?.Invoke($"Bundled mod {name}: installed from this HellForge version.");
        if (name.Equals(EditorFolder, StringComparison.OrdinalIgnoreCase)) EditorData(install, target, changed, log);
    }

    // The shipped files that are missing or differ in target, copied: true when any was.
    static bool CopyShipped(string shipped, string target, List<string> files)
    {
        var changed = false;
        foreach (var relative in files)
        {
            var file = Path.Combine(shipped, relative);
            var destination = Path.Combine(target, relative);
            if (File.Exists(destination) && Same(file, destination)) continue;
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            File.Copy(file, destination, overwrite: true);
            changed = true;
        }
        return changed;
    }

    // Files listed as shipped before that are still inside target, deleted: true when any was.
    static bool DeleteStale(string target, IEnumerable<string> stale)
    {
        var changed = false;
        foreach (var relative in stale)
        {
            var path = Path.GetFullPath(Path.Combine(target, relative));
            if (!path.StartsWith(Path.GetFullPath(target) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) || !File.Exists(path)) continue;
            File.Delete(path);
            changed = true;
        }
        return changed;
    }

    // False when a copy of the player's own (not the one HellForge installed) of the bundled version or newer keeps its
    // place: nothing of theirs is touched, and HellForge's built copy is set aside so the mod is not there twice. Else
    // their older copies, and another mod in the folder this one ships to, are retired and the bundled one goes in.
    static bool Settle(InstallLayout install, string name, string target, Identity bundled, Action<string>? log)
    {
        var copies = Copies(install);
        var own = copies.FirstOrDefault(c => !c.Archive && Path.GetFullPath(c.Path).Equals(Path.GetFullPath(target), StringComparison.OrdinalIgnoreCase));
        var built = own.Path != null && own.Id.Id == bundled.Id && Installed(target);
        var theirs = copies.Where(c => c.Id.Id == bundled.Id && !(built && c.Path == own.Path)).ToList();
        var keeper = theirs.Where(c => c.Id.Version >= bundled.Version).OrderByDescending(c => c.Id.Version).FirstOrDefault();
        if (keeper.Path != null)
        {
            log?.Invoke($"Bundled mod {name}: {Path.GetFileName(keeper.Path)} is your own copy (version {keeper.Id.Text}, not older than the bundled {bundled.Text}), so it is used as it is and HellForge's built copy is not installed beside it.");
            if (built) Retire(install, name, target, own, carry: false, $"your own copy {Path.GetFileName(keeper.Path)} is used instead of HellForge's built one", log);
            return false;
        }
        foreach (var copy in theirs)
            Retire(install, name, target, copy, carry: true, $"it is older than the bundled version {bundled.Text}, which replaces it (a copy of that version or newer would be used instead)", log);
        if (own.Path != null && own.Id.Id != bundled.Id)
            Retire(install, name, target, own, carry: true, $"it is another mod ({own.Id.Id}) in the folder HellForge's built copy goes to", log);
        return true;
    }

    // Whether the copy in a bundled mod's folder is the one HellForge put there: it has the shipped-files list and no
    // sources. Any other copy is the player's own.
    static bool Installed(string folder) => File.Exists(Path.Combine(folder, ListName)) && !Directory.Exists(Path.Combine(folder, "src"));

    // The mods in mods (folders, not hidden ones, and zips) with an identity.
    static List<(string Path, bool Archive, Identity Id, bool Source)> Copies(InstallLayout install)
    {
        var copies = new List<(string Path, bool Archive, Identity Id, bool Source)>();
        if (!Directory.Exists(install.Mods)) return copies;
        foreach (var folder in Directory.GetDirectories(install.Mods).Where(f => !Path.GetFileName(f).StartsWith('.')))
            if (FolderIdentity(folder) is { } id) copies.Add((folder, false, id, Directory.Exists(Path.Combine(folder, "src"))));
        foreach (var archive in Directory.GetFiles(install.Mods, "*.zip"))
            if (ZipIdentity(archive) is { } id) copies.Add((archive, true, id, false));
        return copies;
    }

    // Moved, never deleted: the log line says where it went and why, so the player can find and put back their copy.
    static void Retire(InstallLayout install, string name, string target, (string Path, bool Archive, Identity Id, bool Source) copy, bool carry, string reason, Action<string>? log)
    {
        var (path, archive, id, _) = copy;
        var retired = Directory.CreateDirectory(Path.Combine(install.Root, RetiredFolder)).FullName;
        var label = (archive ? Path.GetFileNameWithoutExtension(path) : Path.GetFileName(path)) + "-" + id.Text;
        var destination = Path.Combine(retired, archive ? label + ".zip" : label);
        for (var n = 2; File.Exists(destination) || Directory.Exists(destination); n++)
            destination = Path.Combine(retired, archive ? $"{label}-{n}.zip" : $"{label}-{n}");
        // what the player saved in a copy of this mod comes along to the bundled one (a zip holds none; none is installed
        // when carry is false); the bundled copy's own saved files win
        var hold = archive || !carry ? null : HoldKept(install, name, path, id);
        if (archive) File.Move(path, destination);
        else Directory.Move(path, destination);
        if (hold != null && Directory.Exists(hold)) { CopyTree(hold, target, overwrite: false); Directory.Delete(hold, recursive: true); }
        log?.Invoke($"Bundled mod {name}: the other copy {Path.GetFileName(path)} (version {id.Text}) was moved to {RetiredFolder}/{Path.GetFileName(destination)}: {reason}.");
    }

    // A folder copy's kept files, held in a temporary folder until the bundled copy is in place: its path, or null when
    // the copy is another mod or the mod keeps nothing.
    static string? HoldKept(InstallLayout install, string name, string path, Identity id)
    {
        if (id.Id != FolderIdentity(Path.Combine(Folder(install), name))?.Id || !Kept.TryGetValue(name, out var kept)) return null;
        var hold = Path.Combine(install.Temp, "bundled-keep-" + Guid.NewGuid().ToString("N")[..8]);
        foreach (var keep in kept)
        {
            var from = Path.Combine(path, keep);
            if (keep.EndsWith('/') && Directory.Exists(from)) CopyTree(from, Path.Combine(hold, keep), overwrite: true);
            else if (File.Exists(from)) { Directory.CreateDirectory(hold); File.Copy(from, Path.Combine(hold, keep)); }
        }
        return hold;
    }

    static void CopyTree(string from, string to, bool overwrite)
    {
        foreach (var file in Directory.GetFiles(from, "*", SearchOption.AllDirectories))
        {
            var into = Path.Combine(to, Path.GetRelativePath(from, file));
            if (!overwrite && File.Exists(into)) continue;
            Directory.CreateDirectory(Path.GetDirectoryName(into)!);
            File.Copy(file, into, overwrite);
        }
    }

    // The catalogue follows the app: written whenever its text is not this HellForge's (a catalogue change ships with an
    // unchanged editor too). Icons and fonts come from the game copy: made when missing, when the editor changed, and when
    // the sprites the app names or the icon file's format did (IconStampFile holds EditorIcons.Stamp).
    public const string IconStampFile = "game-icons.stamp";
    static void EditorData(InstallLayout install, string folder, bool changed, Action<string>? log)
    {
        EditorCatalog.Write(folder);
        // Only from a complete copy: the launcher syncs at start, which can be before or during setup's copy.
        var gameData = Path.Combine(install.Game, "data");
        var icons = Path.Combine(folder, EditorIcons.FileName);
        var stamp = Path.Combine(folder, IconStampFile);
        var current = File.Exists(icons) && !changed && File.Exists(stamp) && File.ReadAllText(stamp) == EditorIcons.Stamp;
        if (current || !SupportedBuild.Current.IsComplete(install.Game)) return;
        var (count, missing) = EditorIcons.Write(gameData, icons);
        var fonts = EditorFonts.Write(gameData, folder);
        File.WriteAllText(stamp, EditorIcons.Stamp);
        log?.Invoke($"In-game editor: {count} icons ({missing.Count} missing) and {fonts.Count} fonts from this installation's game copy.");
    }

    // The files HellForge ships for the bundled mod in modFolder (relative, '/'), or null when it is not one (no
    // app/bundled-mods/<folder> in its installation, or the player's own copy of it). Friends compare these
    // (ModCatalog.Digest), not what each player's copy made or saved there: the editor's settings, presets, icons, fonts.
    public static HashSet<string>? ShippedFiles(string modFolder)
    {
        modFolder = Path.GetFullPath(modFolder);
        if (Directory.Exists(Path.Combine(modFolder, "src")) || Path.GetDirectoryName(Path.GetDirectoryName(modFolder)) is not { } root) return null;
        var shipped = Path.Combine(root, "app", FolderName, Path.GetFileName(modFolder));
        // with an identity, only the copy HellForge installed counts: the player's own in this folder has other files
        return Directory.Exists(shipped) && (FolderIdentity(shipped) == null || Installed(modFolder))
            ? Directory.GetFiles(shipped, "*", SearchOption.AllDirectories).Select(f => Path.GetRelativePath(shipped, f).Replace('\\', '/')).ToHashSet(StringComparer.Ordinal)
            : null;
    }

    static bool Same(string a, string b)
    {
        var x = new FileInfo(a);
        var y = new FileInfo(b);
        if (x.Length != y.Length) return false;
        // File.Copy keeps the write time, so an untouched copy is told without reading it at every start.
        if (x.LastWriteTimeUtc == y.LastWriteTimeUtc) return true;
        using var first = x.OpenRead();
        using var second = y.OpenRead();
        return System.Security.Cryptography.SHA256.HashData(first).AsSpan().SequenceEqual(System.Security.Cryptography.SHA256.HashData(second));
    }
}
