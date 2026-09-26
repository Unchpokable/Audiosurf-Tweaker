using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;

using NUnit.Framework;

using TweakerLuaLinker.Diagnostics;
using TweakerLuaLinker.Linking;

namespace TweakerLuaLinker.Tests
{
    /// <summary>A throwaway project folder: a manifest built from fields, plus whatever files a test writes.</summary>
    internal sealed class TestProject : IDisposable
    {
        public string Root { get; }

        private readonly Dictionary<string, string> _fields = new(StringComparer.Ordinal)
        {
            ["format"] = "1",
            ["id"] = "\"demo\"",
            ["name"] = "\"Demo HUD\"",
            ["author"] = "\"Tester\"",
            ["version"] = "\"1.2.3\"",
            ["description"] = "\"A demo\"",
            ["api"] = "1",
        };

        public TestProject()
        {
            Root = Path.Combine(Path.GetTempPath(), "twllink-tests", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(Root);
            WriteManifest();
        }

        /// <summary>Sets a manifest field to a raw JSON value; null removes it.</summary>
        public TestProject Field(string name, string? rawJson)
        {
            if (rawJson == null)
                _fields.Remove(name);
            else
                _fields[name] = rawJson;
            WriteManifest();
            return this;
        }

        public TestProject RawManifest(string text)
        {
            File.WriteAllText(Path.Combine(Root, "twproject.json"), text);
            return this;
        }

        public TestProject Write(string relativePath, string text)
        {
            return WriteBytes(relativePath, new UTF8Encoding(false).GetBytes(text));
        }

        public TestProject WriteBytes(string relativePath, byte[] bytes)
        {
            var path = Path.Combine(Root, relativePath);
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.WriteAllBytes(path, bytes);
            return this;
        }

        public TestProject Main(string text)
        {
            return Write("src/main.lua", text);
        }

        /// <summary>What the project is checked against; null is the embedded api.json, the game's.</summary>
        public TweakerLuaLinker.Api.ApiDescription? Api { get; set; }

        /// <summary>
        /// For a project whose bundle runs under minilua (<see cref="LuaRunner"/>): its fixtures print
        /// through io.write, which the game's sandbox removes - so the checks are told io exists.
        /// </summary>
        public TestProject ForMinilua()
        {
            Api = LuaRunner.MiniluaApi;
            return this;
        }

        public LinkResult Link()
        {
            return Linker.Link(Root, Api);
        }

        private void WriteManifest()
        {
            var body = string.Join(",\n", _fields.Select(f => $"  \"{f.Key}\": {f.Value}"));
            File.WriteAllText(Path.Combine(Root, "twproject.json"), "{\n" + body + "\n}\n");
        }

        public void Dispose()
        {
            try
            {
                Directory.Delete(Root, true);
            }
            catch (IOException)
            {
            }
        }
    }

    internal static class Assertions
    {
        public static void Succeeded(LinkResult result)
        {
            Assert.That(result.Success, Is.True, "build failed:\n" + Describe(result.Diagnostics));
        }

        public static Diagnostic Single(LinkResult result, string code)
        {
            var matches = result.Diagnostics.Where(d => d.Code == code).ToList();
            Assert.That(matches, Has.Count.EqualTo(1), $"expected exactly one {code}, got:\n" + Describe(result.Diagnostics));
            return matches[0];
        }

        public static string Describe(IEnumerable<Diagnostic> diagnostics)
        {
            var lines = diagnostics.Select(d => $"  {d.File}({d.Line},{d.Column}): {d.Severity} {d.Code}: {d.Message}").ToList();
            return lines.Count == 0 ? "  (no diagnostics)" : string.Join("\n", lines);
        }
    }

    /// <summary>
    /// Runs a bundle under minilua - the plain Lua 5.1 interpreter LuaJIT's own build produces as a host tool.
    /// Tests that need it are ignored, with the reason, when the plugin has not been built on this machine.
    ///
    /// minilua is a build tool, not a full Lua: it has no print, tostring, getfenv, xpcall or math. Fixtures write
    /// through io.write and spell out their own conversions.
    /// </summary>
    internal static class LuaRunner
    {
        private static readonly Lazy<string?> _minilua = new(Find);

        private static readonly Lazy<TweakerLuaLinker.Api.ApiDescription> _miniluaApi = new(() =>
        {
            using var stream = typeof(Linker).Assembly.GetManifestResourceStream(TweakerLuaLinker.Api.ApiDescription.ResourceName)!;
            var json = new StreamReader(stream).ReadToEnd().Replace("\r\n", "\n")
                .Replace("    \"io\",\n", "")
                .Replace("\"globals\": {\n", "\"globals\": {\n    \"io\": { \"kind\": \"table\" },\n    \"io.write\": { \"kind\": \"function\" },\n");
            return TweakerLuaLinker.Api.ApiDescription.Parse(new MemoryStream(Encoding.UTF8.GetBytes(json)));
        });

        /// <summary>The game's API plus io.write, which minilua fixtures print through.</summary>
        public static TweakerLuaLinker.Api.ApiDescription MiniluaApi => _miniluaApi.Value;

        public static string Require()
        {
            var path = _minilua.Value;
            if (path == null)
                Assert.Ignore("minilua.exe not found - build TweakerPlugin (cmake --build --preset x86-release) to run bundles in tests");
            return path!;
        }

        public static (int ExitCode, string Output) Run(string bundle)
        {
            var exe = Require();
            var file = Path.Combine(Path.GetTempPath(), "twllink-tests", Guid.NewGuid().ToString("N") + ".lua");
            Directory.CreateDirectory(Path.GetDirectoryName(file)!);
            File.WriteAllText(file, bundle);
            try
            {
                var start = new ProcessStartInfo(exe, $"\"{file}\"")
                {
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                };
                using var process = Process.Start(start)!;
                var output = process.StandardOutput.ReadToEnd() + process.StandardError.ReadToEnd();
                process.WaitForExit();
                // minilua's stdout is a text-mode C stream: "\n" arrives as "\r\n".
                return (process.ExitCode, output.Replace("\r\n", "\n"));
            }
            finally
            {
                File.Delete(file);
            }
        }

        private static string? Find()
        {
            var root = RepositoryRoot();
            if (root == null)
                return null;
            foreach (var preset in new[] { "x86-release", "x86-debug" })
            {
                var path = Path.Combine(root, "TweakerPlugin", "build", preset, "luajit-gen", "minilua.exe");
                if (File.Exists(path))
                    return path;
            }
            return null;
        }

        public static string? RepositoryRoot()
        {
            for (var dir = new DirectoryInfo(TestContext.CurrentContext.TestDirectory); dir != null; dir = dir.Parent)
            {
                if (File.Exists(Path.Combine(dir.FullName, "Audiosurf SkinChanger.sln")))
                    return dir.FullName;
            }
            return null;
        }
    }
}
