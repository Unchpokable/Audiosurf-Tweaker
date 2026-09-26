using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace TweakerLuaLinker.Project
{
    /// <summary>
    /// `twllink --new`: a project that builds and is set up for an editor from the first minute
    /// (lua-engine-fix-roadmap.md §9.7, §9.8). Templates are written here, in code, rather than as a pile of
    /// files next to the tool - they are a few lines each, and a test builds every one of them.
    ///
    /// The editor side lives in `.twllink/`: the tw definitions for Lua Language Server and the JSON Schema
    /// for twproject.json, both copies of what this twllink was built with. <see cref="RefreshTooling"/>
    /// keeps them current - every build rewrites them when they differ - so updating the Tweaker updates
    /// what the editor knows about the API too.
    /// </summary>
    internal static class Scaffold
    {
        public const string ToolingFolder = ".twllink";
        public const string DefinitionsFile = "tw.def.lua";
        public const string SchemaFile = "twproject.schema.json";

        public static IReadOnlyList<string> Templates { get; } = ["hud", "hook", "empty"];

        /// <summary>Creates the project; null on success, otherwise what stopped it.</summary>
        public static string? Create(string directory, string template, int api)
        {
            if (!Templates.Contains(template))
                return $"unknown template '{template}' - one of: {string.Join(", ", Templates)}";
            if (Directory.Exists(directory) && Directory.EnumerateFileSystemEntries(directory).Any())
                return $"{directory} is not empty - a new project goes into a new or empty folder";

            var folderName = Path.GetFileName(Path.GetFullPath(directory).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
            var id = IdFrom(folderName);
            var name = folderName.Trim().Length > 0 && !folderName.Any(char.IsControl) ? folderName.Trim() : id;

            Directory.CreateDirectory(Path.Combine(directory, "src"));
            Write(directory, Manifest.FileName, ManifestText(id, name, api));
            Write(directory, ".luarc.json", LuaRc);
            Write(directory, ".gitignore", "build/\n");
            foreach (var (file, text) in Sources(template, name))
                Write(directory, file, text);
            Directory.CreateDirectory(Path.Combine(directory, ToolingFolder));
            RefreshTooling(directory);
            return null;
        }

        /// <summary>
        /// Brings `.twllink/` up to date with this twllink, if the project has one. A project without it
        /// (made by hand, or by someone who deleted it) is left alone - it is editor support, not a build
        /// input.
        /// </summary>
        public static void RefreshTooling(string projectRoot)
        {
            var folder = Path.Combine(projectRoot, ToolingFolder);
            if (!Directory.Exists(folder))
                return;

            WriteIfChanged(Path.Combine(folder, DefinitionsFile), Resource(DefinitionsFile));
            WriteIfChanged(Path.Combine(folder, SchemaFile), Resource(SchemaFile));
        }

        public static string Resource(string name)
        {
            using var stream = typeof(Scaffold).Assembly.GetManifestResourceStream(name)
                ?? throw new InvalidOperationException($"{name} is not embedded in twllink - the build is broken");
            return new StreamReader(stream).ReadToEnd().Replace("\r\n", "\n");
        }

        /// <summary>A folder name made into an id: "My HUD!" -> "my_hud".</summary>
        public static string IdFrom(string folderName)
        {
            var builder = new StringBuilder();
            foreach (var c in folderName.ToLowerInvariant())
            {
                if (c is >= 'a' and <= 'z' or >= '0' and <= '9')
                    builder.Append(c);
                else if (builder.Length > 0 && builder[^1] != '_')
                    builder.Append('_');
            }
            var id = builder.ToString().Trim('_');
            if (id.Length > 64)
                id = id[..64].TrimEnd('_');
            return id.Length > 0 ? id : "my_script";
        }

        private static void Write(string root, string relative, string text)
        {
            var path = Path.Combine(root, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.WriteAllText(path, text, new UTF8Encoding(false));
        }

        private static void WriteIfChanged(string path, string text)
        {
            if (File.Exists(path) && File.ReadAllText(path).Replace("\r\n", "\n") == text)
                return;
            File.WriteAllText(path, text, new UTF8Encoding(false));
        }

        private static string ManifestText(string id, string name, int api)
        {
            var author = Environment.UserName;
            if (string.IsNullOrWhiteSpace(author) || author.Any(char.IsControl))
                author = "you";
            return $$"""
                {
                  "$schema": "{{ToolingFolder}}/{{SchemaFile}}",
                  "format": 1,
                  "id": "{{id}}",
                  "name": "{{Escape(name)}}",
                  "author": "{{Escape(author)}}",
                  "version": "0.1.0",
                  "description": "What it does, on one line.",
                  "api": {{api}}
                }

                """;
        }

        private static string Escape(string text)
        {
            return text.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        // Lua Language Server: the game's Lua, modules under src/, the tw definitions, and none of the
        // libraries the sandbox removes - so the editor underlines io.open where the game would fail.
        private const string LuaRc = """
            {
              "$schema": "https://raw.githubusercontent.com/LuaLS/vscode-lua/master/setting/schema.json",
              "runtime.version": "LuaJIT",
              "runtime.path": ["src/?.lua"],
              "runtime.builtin": {
                "io": "disable",
                "debug": "disable",
                "ffi": "disable",
                "jit": "disable",
                "package": "disable"
              },
              "workspace.library": [".twllink"],
              "workspace.ignoreDir": ["build"]
            }

            """;

        private static IEnumerable<(string File, string Text)> Sources(string template, string name)
        {
            var header = $"""
                -- {name}
                --
                -- A twllink project. Modules live in src/ and are pulled in with require("name") - src/draw/rail.lua
                -- is require("draw.rail"). In this folder, `twllink` builds build/<id>.lua and `twllink --install`
                -- also puts it into the game. Every tw.* name has its documentation on hover; the full reference
                -- is Docs/scripting/api-reference.md in the Tweaker's repository.

                """;

            switch (template)
            {
                case "hud":
                    yield return ("src/main.lua", header + """
                        local text = require("text")

                        -- Created once, at the top level: a handle finds its channel the first time it is read and
                        -- remembers it. One created inside on_frame would search for it every frame.
                        local points = tw.float_ch("StatCollector", "Points")

                        tw.on_frame(function()
                            local score = points:get()
                            if score == nil then
                                return -- not in a run, or the game has not loaded that group yet
                            end

                            local x, y = tw.hud.safe()
                            tw.hud.text(x + 20, y + 20, text.score(score), tw.theme("text_primary"))
                        end)

                        """);
                    yield return ("src/text.lua", """
                        -- Text helpers. A module: its locals are its own, and what it returns is what require hands back.

                        local M = {}

                        -- "Score: 1 234 567" - thousands spaced out, because a seven-figure number is not readable at a glance.
                        function M.score(n)
                            local digits = string.format("%.0f", math.abs(n))
                            local out = string.sub(digits, -3)
                            local at = #digits - 3
                            while at > 0 do
                                out = string.sub(digits, math.max(1, at - 2), at) .. " " .. out
                                at = at - 3
                            end
                            return "Score: " .. (n < 0 and "-" or "") .. out
                        end

                        return M

                        """);
                    break;

                case "hook":
                    yield return ("src/main.lua", header + """
                        -- Counts collected blocks by hooking the game's own collection handler, and draws the count.
                        local collected = 0

                        -- "after": the handler has run, so the block really was collected.
                        tw.on_call("TrafficCommander", "Do_CollectCar", "after", function()
                            collected = collected + 1
                        end)

                        -- Restarting a song from the pause menu fires this and nothing else.
                        tw.on_call("StatCollector", "Do_ResetSimpleStats", "after", function()
                            collected = 0
                        end)

                        tw.on_frame(function()
                            local x, y = tw.hud.safe()
                            tw.hud.text(x + 20, y + 20, string.format("Blocks collected: %d", collected), tw.theme("text_primary"))
                        end)

                        """);
                    break;

                default:
                    yield return ("src/main.lua", header + """
                        tw.on_frame(function()
                        end)

                        """);
                    break;
            }
        }
    }
}
