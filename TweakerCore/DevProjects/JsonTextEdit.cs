using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Encodings.Web;
using System.Text.Json;

namespace TweakerCore.DevProjects
{
    /// <summary>
    /// Changes top-level values of a JSON file the user owns by editing its text, not by writing a new file: the
    /// file is theirs, with their comments, their order and their sections, and something that changed a field or two
    /// has no business reformatting the rest. Each changed value replaces exactly the bytes of the old one; a field
    /// that is not there yet is added after the last one, lined up with the others. A BOM stays.
    ///
    /// Comments and trailing commas are read, the way the plugin reads its own JSON files (yyjson with
    /// ALLOW_COMMENTS | ALLOW_TRAILING_COMMAS) - twproject.json, a sky's Config.json and module.json all may have them.
    ///
    /// Values are JSON text: <see cref="String"/>, <see cref="Number"/>, <see cref="Bool"/>, or anything already
    /// serialized. A replaced object or array is replaced whole, comments inside it included.
    /// </summary>
    public static class JsonTextEdit
    {
        private static readonly JsonReaderOptions ReaderOptions = new JsonReaderOptions
        {
            CommentHandling = JsonCommentHandling.Skip,
            AllowTrailingCommas = true
        };

        /// <summary>A string value, trimmed. Relaxed escaping: a name in Cyrillic stays readable instead of turning into \uXXXX.</summary>
        public static string String(string value)
        {
            return "\"" + JsonEncodedText.Encode((value ?? "").Trim(), JavaScriptEncoder.UnsafeRelaxedJsonEscaping) + "\"";
        }

        public static string Number(int value)
        {
            return value.ToString(System.Globalization.CultureInfo.InvariantCulture);
        }

        public static string Bool(bool value)
        {
            return value ? "true" : "false";
        }

        /// <summary>
        /// Writes <paramref name="changes"/> (top-level field → JSON text of its new value) into the file at
        /// <paramref name="path"/>. Throws <see cref="InvalidDataException"/> when the file is not a JSON object.
        /// </summary>
        public static void Update(string path, IReadOnlyList<KeyValuePair<string, string>> changes)
        {
            var bytes = File.ReadAllBytes(path);
            File.WriteAllBytes(path, Edit(bytes, changes, Path.GetFileName(path)));
        }

        /// <summary>The text edit behind <see cref="Update"/>, on the file's bytes; <paramref name="fileName"/> names it in an error.</summary>
        public static byte[] Edit(byte[] bytes, IReadOnlyList<KeyValuePair<string, string>> changes, string fileName)
        {
            var bom = bytes.Length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF ? 3 : 0;
            var json = new ReadOnlySpan<byte>(bytes, bom, bytes.Length - bom);

            // (start, end, replacement) in positions of `json`.
            var edits = new List<(int Start, int End, string Text)>();
            var pending = new Dictionary<string, string>(changes.Count);
            foreach (var change in changes)
                pending[change.Key] = change.Value;

            var reader = new Utf8JsonReader(json, ReaderOptions);
            try
            {
                if (!reader.Read() || reader.TokenType != JsonTokenType.StartObject)
                    throw new InvalidDataException(fileName + " is not a JSON object");
            }
            catch (JsonException exception)
            {
                throw new InvalidDataException(fileName + " is not valid JSON: " + exception.Message, exception);
            }

            var lastValueEnd = (int)reader.BytesConsumed;
            var indent = "  ";
            while (reader.Read())
            {
                if (reader.TokenType == JsonTokenType.EndObject && reader.CurrentDepth == 0)
                    break;
                if (reader.TokenType != JsonTokenType.PropertyName || reader.CurrentDepth != 1)
                    continue;

                indent = IndentBefore(json, (int)reader.TokenStartIndex) ?? indent;
                var name = reader.GetString();
                reader.Read();
                var valueStart = (int)reader.TokenStartIndex;
                if (reader.TokenType == JsonTokenType.StartObject || reader.TokenType == JsonTokenType.StartArray)
                    reader.Skip();
                lastValueEnd = (int)reader.BytesConsumed;

                if (name != null && pending.TryGetValue(name, out var value))
                {
                    edits.Add((valueStart, lastValueEnd, value));
                    pending.Remove(name);
                }
            }

            // Fields the file does not have yet go after the last one, in the order they were given.
            if (pending.Count > 0)
            {
                var added = new StringBuilder();
                var first = lastValueEnd == json.IndexOf((byte)'{') + 1;
                foreach (var change in changes.Where(c => pending.ContainsKey(c.Key)))
                {
                    added.Append(first ? "\n" : ",\n").Append(indent).Append(String(change.Key)).Append(": ").Append(change.Value);
                    first = false;
                }
                edits.Add((lastValueEnd, lastValueEnd, added.ToString()));
            }

            var result = new List<byte>(bytes.Length + 64);
            result.AddRange(bytes.Take(bom));
            var at = 0;
            foreach (var edit in edits.OrderBy(e => e.Start))
            {
                result.AddRange(json.Slice(at, edit.Start - at).ToArray());
                result.AddRange(Encoding.UTF8.GetBytes(edit.Text));
                at = edit.End;
            }
            result.AddRange(json.Slice(at).ToArray());
            return result.ToArray();
        }

        /// <summary>The whitespace a property line starts with, so an added field lines up with the others.</summary>
        private static string IndentBefore(ReadOnlySpan<byte> json, int position)
        {
            var start = position;
            while (start > 0 && (json[start - 1] == (byte)' ' || json[start - 1] == (byte)'\t'))
                start--;
            if (start > 0 && json[start - 1] != (byte)'\n')
                return null;
            return Encoding.ASCII.GetString(json.Slice(start, position - start));
        }
    }
}
