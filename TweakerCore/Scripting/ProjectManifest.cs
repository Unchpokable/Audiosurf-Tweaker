using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using TweakerCore.DevProjects;

namespace TweakerCore.Scripting
{
    /// <summary>
    /// The fields of twproject.json the Script Dev tab edits - id, name, author, version, description - checked by
    /// twllink's own rules and written back without disturbing anything else in the file.
    ///
    /// The rules are twllink's (TweakerLuaLinker/Project/ManifestReader.cs), repeated here so the form can say what is
    /// wrong before a build does. The reasons behind the two strict ones:
    /// - id is the script's file name in the game (&lt;id&gt;.lua) and its identity - the plugin and scripts.cfg key a
    ///   script by it, and scripts.cfg compares names case-sensitively while the file system does not; lowercase
    ///   letters, digits and '_' leave no two spellings of one name;
    /// - version is MAJOR.MINOR.PATCH, three whole numbers without leading zeros.
    /// name, author and description are free text on one line - they are written into the script's header, which
    /// the plugin reads line by line.
    ///
    /// Writing is an edit of the text, not a new file (<see cref="JsonTextEdit"/>): a manifest is the user's file, with
    /// their comments, their order and their build/diagnostics sections, and a form that touched five fields has no
    /// business reformatting the rest.
    /// </summary>
    public static class ProjectManifest
    {
        public const string Id = "id";
        public const string Name = "name";
        public const string Author = "author";
        public const string Version = "version";
        public const string Description = "description";

        private static readonly Regex IdPattern = new Regex("^[a-z0-9_]{1,64}$", RegexOptions.CultureInvariant);
        private static readonly Regex VersionPattern = new Regex(@"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$", RegexOptions.CultureInvariant);

        /// <summary>What is wrong with one field's value, in a sentence; null when twllink would take it.</summary>
        public static string Problem(string field, string value)
        {
            value ??= "";
            switch (field)
            {
                case Id:
                    return IdPattern.IsMatch(value)
                        ? null
                        : "Only a-z, 0-9 and _, 1-64 characters.";
                case Version:
                    return VersionPattern.IsMatch(value.Trim())
                        ? null
                        : "Use MAJOR.MINOR.PATCH - three whole numbers without leading zeros, like 1.0.0.";
                default:
                    if (value.Trim().Length == 0)
                        return "Must not be empty.";
                    return value.Any(char.IsControl) ? "Must be a single line." : null;
            }
        }

        /// <summary>
        /// "My HUD!" -> "my_hud": what a display name becomes as an id - the same derivation as twllink's
        /// Scaffold.IdFrom, so a project named in the form gets the id twllink would have given its folder.
        /// </summary>
        public static string IdFrom(string name)
        {
            var builder = new StringBuilder();
            foreach (var c in (name ?? "").ToLowerInvariant())
            {
                if (c >= 'a' && c <= 'z' || c >= '0' && c <= '9')
                    builder.Append(c);
                else if (builder.Length > 0 && builder[builder.Length - 1] != '_')
                    builder.Append('_');
            }
            var id = builder.ToString().Trim('_');
            if (id.Length > 64)
                id = id.Substring(0, 64).TrimEnd('_');
            return id;
        }

        /// <summary>
        /// Writes <paramref name="changes"/> (field → new value) into the project's twproject.json. Values are
        /// written as they are given - trimmed, but not checked: <see cref="Problem"/> is the caller's to ask first.
        /// </summary>
        public static void Update(string projectRoot, IReadOnlyDictionary<string, string> changes)
        {
            var path = Path.Combine(projectRoot, ScriptProjects.ManifestFileName);
            var bytes = File.ReadAllBytes(path);
            File.WriteAllBytes(path, Edit(bytes, changes));
        }

        /// <summary>The text edit behind <see cref="Update"/>, on the file's bytes: every field of this manifest is a string.</summary>
        internal static byte[] Edit(byte[] bytes, IReadOnlyDictionary<string, string> changes)
        {
            var values = changes.Select(change => new KeyValuePair<string, string>(change.Key, JsonTextEdit.String(change.Value))).ToList();
            return JsonTextEdit.Edit(bytes, values, ScriptProjects.ManifestFileName);
        }
    }
}
