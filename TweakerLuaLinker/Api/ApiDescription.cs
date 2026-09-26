using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace TweakerLuaLinker.Api
{
    /// <summary>One name a script can reach: a global, a field of one, or a method of a handle.</summary>
    internal sealed record ApiEntry(string Kind, IReadOnlyList<string>? Params, bool Vararg, string? Returns, int Since)
    {
        public bool IsTable => Kind == "table";
        public bool IsFunction => Kind == "function";
    }

    /// <summary>
    /// Docs/scripting/api.json: what a script's environment contains, generated from the plugin's prelude
    /// by harness/lua (lua-engine-fix-roadmap.md §9.8). Embedded into twllink at build time, so the names
    /// it checks are the ones of the plugin it was built with; --api points it at another file.
    ///
    /// Names are flat and dotted: "tw.hud.text", "string.format", "os.clock". A table's fields are exactly
    /// the entries one dot below it - the generator walks the whole table, so a name missing from a table
    /// that is present is a name the script will get nil for.
    /// </summary>
    internal sealed class ApiDescription
    {
        public const string ResourceName = "api.json";

        public int Api { get; }
        public IReadOnlyDictionary<string, ApiEntry> Globals { get; }

        /// <summary>"FloatHandle:get" and the like.</summary>
        public IReadOnlyDictionary<string, ApiEntry> Types { get; }

        /// <summary>What the sandbox takes away: "io", "os.execute".</summary>
        public IReadOnlySet<string> Removed { get; }

        private ApiDescription(int api, Dictionary<string, ApiEntry> globals, Dictionary<string, ApiEntry> types, HashSet<string> removed)
        {
            Api = api;
            Globals = globals;
            Types = types;
            Removed = removed;
        }

        private static readonly Lazy<ApiDescription> _embedded = new(() =>
        {
            using var stream = typeof(ApiDescription).Assembly.GetManifestResourceStream(ResourceName)
                ?? throw new InvalidOperationException($"{ResourceName} is not embedded in twllink - the build is broken");
            return Parse(stream);
        });

        /// <summary>The API this twllink was built with.</summary>
        public static ApiDescription Embedded => _embedded.Value;

        public static ApiDescription Load(string path)
        {
            using var stream = File.OpenRead(path);
            return Parse(stream);
        }

        /// <summary>Throws JsonException or InvalidDataException on a file that is not an api.json.</summary>
        public static ApiDescription Parse(Stream stream)
        {
            using var document = JsonDocument.Parse(stream);
            var root = document.RootElement;
            if (root.GetProperty("format").GetInt32() != 1)
                throw new InvalidDataException("unsupported api.json format");

            return new ApiDescription(
                root.GetProperty("api").GetInt32(),
                ReadEntries(root.GetProperty("globals")),
                ReadEntries(root.GetProperty("types")),
                root.GetProperty("removed").EnumerateArray().Select(e => e.GetString()!).ToHashSet(StringComparer.Ordinal));
        }

        private static Dictionary<string, ApiEntry> ReadEntries(JsonElement obj)
        {
            var result = new Dictionary<string, ApiEntry>(StringComparer.Ordinal);
            foreach (var property in obj.EnumerateObject())
            {
                var e = property.Value;
                result[property.Name] = new ApiEntry(
                    e.GetProperty("kind").GetString()!,
                    e.TryGetProperty("params", out var p) ? p.EnumerateArray().Select(x => x.GetString()!).ToList() : null,
                    e.TryGetProperty("vararg", out var v) && v.GetBoolean(),
                    e.TryGetProperty("returns", out var r) ? r.GetString() : null,
                    e.TryGetProperty("since", out var s) ? s.GetInt32() : 1);
            }
            return result;
        }

        /// <summary>The names one dot below a table: Fields("tw.hud") -> text, rect, corners, ...</summary>
        public IEnumerable<string> Fields(string table)
        {
            var prefix = table + ".";
            return Globals.Keys
                .Where(k => k.StartsWith(prefix, StringComparison.Ordinal) && k.IndexOf('.', prefix.Length) < 0)
                .Select(k => k[prefix.Length..]);
        }
    }
}
