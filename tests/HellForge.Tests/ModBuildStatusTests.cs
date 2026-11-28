using System.Text.Json;
using HellForge.Core;

// Where a source mod's build stands (ModBuilds.Status), as the Mods page and HellForge.Cli's mods show it. The states
// that need a compiled build (built, did-not-compile) are covered with Zig in ModKitTests' source mod build.
static class ModBuildStatusTests
{
    public static void Register(Action<string, Action> test, Action<bool, string> check, Action<Action> reject, string root)
    {
        test("Source mod build progress is written whole, and a file that cannot be replaced loses one line, not the build", () =>
        {
            var folder = Directory.CreateDirectory(Path.Combine(root, "build-progress")).FullName;
            var file = Path.Combine(folder, "mod-builds.json");
            new ModBuildProgress("compiling", 1, 3, "tester.one").WriteTo(file);
            using (var written = JsonDocument.Parse(File.ReadAllText(file)))
                check(written.RootElement.GetProperty("stage").GetString() == "compiling" && written.RootElement.GetProperty("done").GetInt32() == 1 &&
                    written.RootElement.GetProperty("total").GetInt32() == 3 && written.RootElement.GetProperty("mod").GetString() == "tester.one",
                    "The progress file does not hold its stage, counts and mod.");
            check(!File.Exists(file + ".partial"), "The partial file stayed after a write.");

            // a folder in the file's place cannot be replaced on any system
            var blocked = Directory.CreateDirectory(Path.Combine(folder, "blocked.json")).FullName;
            new ModBuildProgress("done", 3, 3, "").WriteTo(blocked);
            check(Directory.Exists(blocked) && !File.Exists(blocked + ".partial"), "A progress file that could not be replaced left its partial file.");
            if (!OperatingSystem.IsWindows()) return;
            // held open without delete sharing, as a scanner does: Windows refuses the move with access denied
            using (new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.Read))
                new ModBuildProgress("done", 3, 3, "").WriteTo(file);
            check(File.ReadAllText(file).Contains("compiling") && !File.Exists(file + ".partial"), "A held progress file was replaced or left its partial file.");
        });

        test("A source mod's build reads as not built, changed (keeping its old error) or failed in the tools; other mods have none", () =>
        {
            var repository = ModSdk.Find(Directory.GetCurrentDirectory())!;
            var install = new InstallLayout(Path.Combine(root, "build-status"));
            install.Create("12345");
            var folder = ModKit.Create(repository, install.Mods, "tester.status-mod", "Status Mod");
            var mod = ModCatalog.Parse(folder, EntityProfile.Current);
            check(ModBuilds.Status(install, mod with { HasSource = false }, repository, true) == null, "A mod without sources has a build state.");
            check(ModBuilds.Status(install, mod, repository, true)?.State == "not-built", "A mod never built is not 'not-built'.");

            void Record(string? error)
            {
                var cache = Directory.CreateDirectory(Path.Combine(ModBuilds.Folder(install), mod.Id)).FullName;
                File.WriteAllText(Path.Combine(cache, ModBuilds.RecordName), JsonSerializer.Serialize(
                    new ModBuildRecord("an old key", "old sources", "status_mod.dll", null, error, new DateTime(2026, 11, 1, 12, 0, 0, DateTimeKind.Utc))));
            }
            Record(null);
            var changed = ModBuilds.Status(install, mod, repository, true);
            check(changed is { State: "changed", Error: null } && changed.Built == new DateTime(2026, 11, 1, 12, 0, 0, DateTimeKind.Utc), "Another key is not 'changed'.");
            Record("src/status_mod.c:3: error: use of undeclared identifier 'missing'");
            check(ModBuilds.Status(install, mod, repository, true) is { State: "changed", Error: not null }, "A failed build with changed sources lost its error.");
            Record("error: sub-compilation of compiler_rt failed");
            check(ModBuilds.Status(install, mod, repository, true)?.State == "tools-failed", "A failure in Zig's own libraries is not 'tools-failed'.");
        });
    }
}
