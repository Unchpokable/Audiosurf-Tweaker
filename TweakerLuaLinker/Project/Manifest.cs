using System.Collections.Generic;

namespace TweakerLuaLinker.Project
{
    internal enum CommentMode
    {
        Keep,
        Strip,
    }

    /// <summary>
    /// twproject.json, validated (lua-engine-fix-roadmap.md §9.4). Everything here is a fact about the
    /// project; nothing is a fact about the machine it is built on.
    /// </summary>
    internal sealed class Manifest
    {
        public const string FileName = "twproject.json";

        /// <summary>The only manifest format this linker reads.</summary>
        public const int CurrentFormat = 1;

        public required string Id { get; init; }
        public required string Name { get; init; }
        public required string Author { get; init; }
        public required string Version { get; init; }
        public required string Description { get; init; }
        public required int Api { get; init; }

        public CommentMode Comments { get; init; } = CommentMode.Keep;

        /// <summary>Project-relative paths, forward slashes, in manifest order.</summary>
        public IReadOnlyList<string> Embed { get; init; } = [];

        public bool WarningsAsErrors { get; init; }
        public IReadOnlyCollection<string> Suppress { get; init; } = [];

        /// <summary>The file name the plugin sees - and keys "disabled" by, so it is the script's identity.</summary>
        public string OutputFileName => Id + ".lua";
    }
}
