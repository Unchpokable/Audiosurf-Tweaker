using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace TweakerCore.Configuration
{
    public static class SteamLibraryFolders
    {
        /// <summary>Reads the text KeyValues1 subset used by Steam libraryfolders.vdf.</summary>
        public static IEnumerable<string> AudiosurfLibraries(string text)
        {
            var entries = new Reader(text).Read();
            var folders = entries.FirstOrDefault(e => e.Key.Equals("libraryfolders", StringComparison.OrdinalIgnoreCase))?.Children;
            if (folders == null)
                yield break;

            foreach (var folder in folders)
            {
                if (!uint.TryParse(folder.Key, out _))
                    continue;

                if (folder.Children == null)
                {
                    // Old Steam layout has no apps index; check its candidate installation on disk.
                    if (!string.IsNullOrEmpty(folder.Value)
                        && Directory.Exists(Path.Combine(folder.Value, "steamapps", "common", "Audiosurf")))
                        yield return folder.Value;
                    continue;
                }

                var path = folder.Children.FirstOrDefault(e => e.Key == "path")?.Value;
                var apps = folder.Children.FirstOrDefault(e => e.Key == "apps")?.Children;
                if (!string.IsNullOrEmpty(path) && apps?.Any(e => e.Key == "12900") == true)
                    yield return path;
            }
        }

        private sealed record Entry(string Key, string Value, List<Entry> Children);

        private sealed class Reader(string text)
        {
            private int _position;

            public List<Entry> Read() => ReadObject(false, 0);

            private List<Entry> ReadObject(bool nested, int depth)
            {
                if (depth > 64)
                    throw Error("Excessive nesting");

                var entries = new List<Entry>();
                while (true)
                {
                    SkipWhitespace();
                    if (_position == text.Length)
                    {
                        if (nested)
                            throw Error("Missing closing brace");
                        return entries;
                    }

                    if (text[_position] == '}')
                    {
                        if (!nested)
                            throw Error("Unexpected closing brace");
                        _position++;
                        return entries;
                    }

                    var key = ReadToken();
                    SkipWhitespace();
                    if (_position < text.Length && text[_position] == '{')
                    {
                        _position++;
                        entries.Add(new Entry(key, null, ReadObject(true, depth + 1)));
                    }
                    else
                    {
                        entries.Add(new Entry(key, ReadToken(), null));
                    }
                }
            }

            private string ReadToken()
            {
                SkipWhitespace();
                if (_position == text.Length || text[_position] is '{' or '}')
                    throw Error("Expected a key or value");
                if (text[_position] is '#' or '[' or ']')
                    throw Error("KeyValues directives and conditions are unsupported");

                var value = new StringBuilder();
                if (text[_position] == '"')
                {
                    _position++;
                    while (_position < text.Length)
                    {
                        var character = text[_position++];
                        if (character == '"')
                            return value.ToString();

                        // Steam escapes backslashes and quotes. Keep unknown escapes literally,
                        // including Windows paths written with single backslashes.
                        if (character == '\\' && _position < text.Length && text[_position] is '\\' or '"')
                            character = text[_position++];
                        value.Append(character);
                    }

                    throw Error("Unterminated quoted string");
                }

                while (_position < text.Length && !char.IsWhiteSpace(text[_position]) && text[_position] is not '{' and not '}')
                {
                    if (text[_position] is '[' or ']')
                        throw Error("KeyValues conditions are unsupported");
                    if (text[_position] == '/' && _position + 1 < text.Length && text[_position + 1] == '/')
                        break;
                    value.Append(text[_position++]);
                }

                if (value.Length == 0)
                    throw Error("Expected a token");
                return value.ToString();
            }

            private void SkipWhitespace()
            {
                while (_position < text.Length)
                {
                    if (char.IsWhiteSpace(text[_position]) || text[_position] == '\uFEFF')
                    {
                        _position++;
                    }
                    else if (text[_position] == '/' && _position + 1 < text.Length && text[_position + 1] == '/')
                    {
                        while (_position < text.Length && text[_position] != '\n')
                            _position++;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            private FormatException Error(string message) => new($"{message} at character {_position}.");
        }
    }
}
