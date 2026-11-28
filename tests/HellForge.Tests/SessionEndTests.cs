using System.Text.Json.Nodes;
using HellForge.Core;
using HellForge.Session;

// How a session's end goes into result.json (src/HellForge.Session/SessionEnd.cs). A Linux sandboxed session that a mod sync
// stops must still name the restart the launcher carries out, although the sandbox supervisor stops the game on the same
// signal before the session runner reads it; and HellForge's own stop is not a crash.
static class SessionEndTests
{
    public static void Register(Action<string, Action> test, Action<bool, string> check, Action<Action> reject, string root)
    {
        test("Session end: a stop signal is recorded, a mod sync's as the launcher's restart, stop-now as ending the game at once", () =>
        {
            var sync = new JsonObject();
            check(!SessionEnd.MarkStop(sync, "mod-sync"), "A mod sync's stop asked to end the game at once.");
            check(sync["restart"]?.GetValue<string>() == "mod-sync" && sync["observationStoppedBySignal"]?.GetValue<bool>() == true,
                "A mod sync's stop was not recorded as the launcher's restart: " + sync.ToJsonString());

            var stop = new JsonObject();
            check(!SessionEnd.MarkStop(stop, "stop") && stop["restart"] == null && stop["observationStoppedBySignal"]?.GetValue<bool>() == true,
                "The launcher's STOP was recorded wrong: " + stop.ToJsonString());

            var now = new JsonObject();
            check(SessionEnd.MarkStop(now, SessionRunner.StopNowSignal) && now["restart"] == null, "stop-now did not end the game at once: " + now.ToJsonString());
        });

        test("Session end: HellForge's own stop (0xB7520003) and an ordinary exit are no crash; a crash code, crash line or dump are", () =>
        {
            check(!SessionEnd.Crashed(unchecked((int)0xB7520003), false, false), "HellForge's own stop counted as a crash.");
            check(!SessionEnd.Crashed(0, false, false) && !SessionEnd.Crashed(1, false, false) && !SessionEnd.Crashed(null, false, false),
                "An ordinary exit counted as a crash.");
            check(SessionEnd.Crashed(unchecked((int)0xC0000005), false, false), "An access violation did not count as a crash.");
            check(SessionEnd.Crashed(unchecked((int)0xB7520003), true, false) && SessionEnd.Crashed(0, false, true),
                "The mod loader's crash line or a dump did not count as a crash.");
        });

        test("Session reads: a read that fails once while the game runs is tried again, and a second failure ends it", () =>
        {
            var calls = 0;
            check(Tools.Retried(() => ++calls == 1 ? throw new SessionFailure("timed out") : 7) == 7 && calls == 2, "A read that failed once was not tried again.");
            calls = 0;
            check(Tools.Retried(() => { calls++; return 3; }) == 3 && calls == 1, "A read that worked ran twice.");
            calls = 0;
            var failed = false;
            try { Tools.Retried<int>(() => { calls++; throw new SessionFailure("timed out"); }); }
            catch (SessionFailure) { failed = true; }
            check(failed && calls == 2, "A read that failed twice did not fail after its second try.");
        });

        test("Backend audit: a half-written last line waits for the next read, and a line that is not JSON is passed over", () =>
        {
            var log = Path.Combine(Directory.CreateDirectory(Path.Combine(root, "backend-audit")).FullName, "requests.jsonl");
            File.WriteAllText(log, "{\"n\":1}\nnot json\n\r\n{\"n\":2}\r\n{\"n\":");
            long offset = 0;
            var rows = Tools.BackendRequests(log, ref offset);
            check(rows.Count == 2 && rows[1]["n"]?.GetValue<int>() == 2, $"Read {rows.Count} complete records, not 2.");
            check(offset == new FileInfo(log).Length - 5, $"The offset {offset} is not at the start of the half-written line.");
            check(Tools.BackendRequests(log, ref offset).Count == 0, "The half-written line was read.");
            File.AppendAllText(log, "3}\n");
            rows = Tools.BackendRequests(log, ref offset);
            check(rows.Count == 1 && rows[0]["n"]?.GetValue<int>() == 3, "The line was not read once it was complete.");
            check(offset == new FileInfo(log).Length && Tools.BackendRequests(log, ref offset).Count == 0, "A record was read twice.");
        });
    }
}
