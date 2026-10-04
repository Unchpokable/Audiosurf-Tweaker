using System;
using System.Globalization;
using System.IO;
using System.Text;

namespace TweakerCore.Scripting
{
    /// <summary>
    /// The <c>-- @key value</c> lines at the top of a script, read without running it - a port of the plugin's
    /// <c>read_header</c> (TweakerPlugin/src/lua/lua_script.cxx), and deliberately a faithful one: the Scripts
    /// tab of TweakerUI and the Scripts tab of the overlay must call a script by the same name.
    ///
    /// The quirks that come with that are the plugin's, kept on purpose: scanning stops at the first line that is
    /// neither blank nor a comment; a key with no value is skipped; a repeated key takes the last value; an
    /// <c>@api</c> line that is not a whole positive number is ignored; a UTF-8 byte order mark is not skipped, so a file that starts with one has no header
    /// (the plugin reads bytes, and its first line then does not start with "--").
    /// </summary>
    public sealed class ScriptHeader
    {
        /// <summary><c>@name</c>, or the file name without .lua when the header does not say.</summary>
        public string Name { get; private set; }

        public string Author { get; private set; } = "";
        public string Version { get; private set; } = "";
        public string Description { get; private set; } = "";

        /// <summary><c>@api</c>: the scripting API the script needs; 0 when it does not say.</summary>
        public int Api { get; private set; }

        /// <summary>
        /// <c>@bundle</c>, which twllink writes into every script it builds ("twllink 1.1, 17 modules"); empty
        /// for a script written by hand. The plugin does not read it - it is for tools.
        /// </summary>
        public string Bundle { get; private set; } = "";

        /// <summary>Whether twllink built this script from a project.</summary>
        public bool IsBundle => Bundle.Length > 0;

        /// <summary>
        /// Reads the header of the script at <paramref name="path"/>. An unreadable file gives a header with only
        /// the name, which is what the plugin shows for it too.
        /// </summary>
        public static ScriptHeader Read(string path)
        {
            var header = new ScriptHeader { Name = Path.GetFileNameWithoutExtension(path) };

            StreamReader reader;
            try
            {
                reader = new StreamReader(path, new UTF8Encoding(false), detectEncodingFromByteOrderMarks: false);
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                return header;
            }

            using (reader)
            {
                try
                {
                    string line;
                    while ((line = reader.ReadLine()) != null)
                    {
                        if (!header.Take(line))
                            break;
                    }
                }
                catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
                {
                }
            }
            return header;
        }

        /// <summary>One line of the file; false once the header is over.</summary>
        private bool Take(string line)
        {
            var text = line.AsSpan().Trim(" \t\r");
            if (text.IsEmpty)
                return true;
            if (!text.StartsWith("--", StringComparison.Ordinal))
                return false;

            text = text.Slice(2).TrimStart(" \t-");
            if (text.IsEmpty || text[0] != '@')
                return true;
            text = text.Slice(1);

            var space = text.IndexOfAny(' ', '\t');
            if (space < 0)
                return true;

            var key = text.Slice(0, space);
            var value = text.Slice(space + 1).TrimStart(" \t");
            if (value.IsEmpty)
                return true;

            if (key.SequenceEqual("name"))
                Name = value.ToString();
            else if (key.SequenceEqual("author"))
                Author = value.ToString();
            else if (key.SequenceEqual("version"))
                Version = value.ToString();
            else if (key.SequenceEqual("description"))
                Description = value.ToString();
            else if (key.SequenceEqual("bundle"))
                Bundle = value.ToString();
            else if (key.SequenceEqual("api") && int.TryParse(value, NumberStyles.None, CultureInfo.InvariantCulture, out var api) && api > 0)
                Api = api;
            return true;
        }
    }
}
