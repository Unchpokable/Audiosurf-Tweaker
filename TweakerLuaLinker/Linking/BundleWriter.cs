using System.Collections.Generic;
using System.Text;

namespace TweakerLuaLinker.Linking
{
    /// <summary>One line of the .map file: from bundle line <see cref="BundleLine"/> on, lines come from <see cref="File"/>.</summary>
    internal readonly record struct MapEntry(int BundleLine, string File, int SourceLine);

    /// <summary>
    /// Writes the bundle one line at a time and keeps the line map as it goes - the map is a by-product of
    /// writing, not a separate calculation (lua-engine-fix-roadmap.md §9.6). Whatever a transformation did
    /// to a module's lines, the map describes exactly the file that was written, because it records lines
    /// instead of predicting them.
    ///
    /// A map entry starts wherever the "next bundle line is the next source line" run breaks: a module
    /// begins, a stripped comment took lines out, or the bundle's own lines begin. Own lines are mapped to
    /// the file "-", which the plugin leaves as bundle positions.
    /// </summary>
    internal sealed class BundleWriter
    {
        /// <summary>The map format version, the first line of every .map file.</summary>
        public const string MapHeader = "# twllink 1";

        public const string OwnFile = "-";

        public const string HashPrefix = "# bundle fnv1a64 ";

        /// <summary>
        /// FNV-1a 64 over the bytes, carriage returns skipped: git on Windows may check the same bundle out
        /// with CRLF, and that must not make its map look stale. lua_linemap.cxx computes the same.
        /// </summary>
        public static ulong BundleHash(byte[] bytes)
        {
            var hash = 0xCBF29CE484222325UL;
            foreach (var b in bytes)
            {
                if (b == (byte)'\r')
                    continue;
                hash ^= b;
                hash *= 0x100000001B3UL;
            }
            return hash;
        }

        private readonly StringBuilder _text = new();
        private readonly List<MapEntry> _map = new();
        private int _line;
        private string? _lastFile;
        private int _lastSourceLine;

        public int LineCount => _line;

        public IReadOnlyList<MapEntry> Map => _map;

        /// <summary>A line that belongs to the bundle itself: header, shim, embedded text, module wrappers.</summary>
        public void Own(string text)
        {
            _line++;
            if (_lastFile != OwnFile)
            {
                _map.Add(new MapEntry(_line, OwnFile, 0));
                _lastFile = OwnFile;
            }
            Append(text);
        }

        public void Source(string file, int sourceLine, string text)
        {
            _line++;
            if (_lastFile != file || sourceLine != _lastSourceLine + 1)
                _map.Add(new MapEntry(_line, file, sourceLine));

            _lastFile = file;
            _lastSourceLine = sourceLine;
            Append(text);
        }

        /// <summary>LF throughout: the bundle is generated, and one line ending keeps it byte-for-byte reproducible.</summary>
        private void Append(string text)
        {
            _text.Append(text).Append('\n');
        }

        public string Text()
        {
            return _text.ToString();
        }

        /// <summary>
        /// The map names the bundle it belongs to by hash, so the plugin can refuse a map left beside a
        /// script someone edited by hand - a map that lies is worse than none (lua_linemap.hxx).
        /// </summary>
        public string MapText()
        {
            var builder = new StringBuilder();
            builder.Append(MapHeader).Append('\n');
            builder.Append(HashPrefix).Append(BundleHash(Encoding.UTF8.GetBytes(Text())).ToString("x16")).Append('\n');
            foreach (var entry in _map)
                builder.Append(entry.BundleLine).Append('\t').Append(entry.File).Append('\t').Append(entry.SourceLine).Append('\n');
            return builder.ToString();
        }
    }
}
