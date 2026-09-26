using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

using TweakerLuaLinker.Diagnostics;

namespace TweakerLuaLinker.Project
{
    /// <summary>
    /// Reads and validates twproject.json. JsonDocument rather than a serializer: no reflection (the linker
    /// is published Native AOT and has no trim warnings to keep it that way), and every rule below wants to
    /// say which field is wrong, which a deserializer would have thrown away.
    ///
    /// Reports every problem it finds rather than stopping at the first, so one build shows the author the
    /// whole list.
    /// </summary>
    internal static class ManifestReader
    {
        private static readonly JsonDocumentOptions _options = new()
        {
            CommentHandling = JsonCommentHandling.Skip,
            AllowTrailingCommas = true,
        };

        private static readonly HashSet<string> _topLevel = new(StringComparer.Ordinal)
        {
            "$schema", "format", "id", "name", "author", "version", "description", "api", "build", "diagnostics",
        };

        private static readonly HashSet<string> _buildFields = new(StringComparer.Ordinal) { "comments", "embed" };
        private static readonly HashSet<string> _diagnosticsFields = new(StringComparer.Ordinal) { "warnings_as_errors", "suppress" };

        // For the test that holds Docs/scripting/twproject.schema.json to the same fields.
        internal static IReadOnlySet<string> TopLevelFields => _topLevel;
        internal static IReadOnlySet<string> BuildFields => _buildFields;
        internal static IReadOnlySet<string> DiagnosticsFields => _diagnosticsFields;

        public static Manifest? Read(string manifestPath, DiagnosticBag bag)
        {
            const string file = Manifest.FileName;

            string text;
            try
            {
                text = File.ReadAllText(manifestPath);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                bag.Add(DiagnosticCodes.ManifestMissing, file, $"cannot read the project manifest: {e.Message}");
                return null;
            }

            JsonDocument document;
            try
            {
                document = JsonDocument.Parse(text, _options);
            }
            catch (JsonException e)
            {
                var line = (int)(e.LineNumber ?? -1) + 1;
                var column = (int)(e.BytePositionInLine ?? -1) + 1;
                bag.Add(DiagnosticCodes.ManifestSyntax, file, line, column, $"twproject.json is not valid JSON: {FirstSentence(e.Message)}");
                return null;
            }

            using (document)
            {
                var root = document.RootElement;
                if (root.ValueKind != JsonValueKind.Object)
                {
                    bag.Add(DiagnosticCodes.ManifestSyntax, file, "twproject.json must contain a JSON object");
                    return null;
                }

                ReportUnknown(root, _topLevel, "", bag);

                var format = RequiredInt(root, "format", bag);
                if (format != null && format != Manifest.CurrentFormat)
                {
                    bag.Add(DiagnosticCodes.ManifestFormat, file,
                        $"manifest format {format} is not supported by this twllink (it reads format {Manifest.CurrentFormat})");
                    return null;
                }

                var id = RequiredString(root, "id", bag);
                if (id != null && !IsValidId(id))
                    bag.Add(DiagnosticCodes.ManifestId, file,
                        $"id '{id}' must be 1-64 characters of a-z, 0-9 and '_' - it becomes the file name '{id}.lua'");

                var name = RequiredText(root, "name", bag);
                var author = RequiredText(root, "author", bag);
                var description = RequiredText(root, "description", bag);

                var version = RequiredString(root, "version", bag);
                if (version != null && !IsValidVersion(version))
                    bag.Add(DiagnosticCodes.ManifestVersion, file,
                        $"version '{version}' must be MAJOR.MINOR.PATCH - three whole numbers without leading zeros, e.g. 1.7.0");

                var api = RequiredInt(root, "api", bag);
                if (api != null && api < 1)
                    bag.Add(DiagnosticCodes.ManifestApi, file, $"api must be a positive whole number, got {api}");

                var comments = CommentMode.Keep;
                var embed = new List<string>();
                if (Section(root, "build", bag) is { } build)
                {
                    ReportUnknown(build, _buildFields, "build.", bag);
                    comments = ReadComments(build, bag);
                    embed = ReadStringArray(build, "embed", "build.embed", bag);
                }

                var warningsAsErrors = false;
                var suppress = new List<string>();
                if (Section(root, "diagnostics", bag) is { } diagnostics)
                {
                    ReportUnknown(diagnostics, _diagnosticsFields, "diagnostics.", bag);
                    if (diagnostics.TryGetProperty("warnings_as_errors", out var wae))
                    {
                        if (wae.ValueKind is JsonValueKind.True or JsonValueKind.False)
                            warningsAsErrors = wae.GetBoolean();
                        else
                            bag.Add(DiagnosticCodes.ManifestFieldType, file, "diagnostics.warnings_as_errors must be true or false");
                    }

                    foreach (var code in ReadStringArray(diagnostics, "suppress", "diagnostics.suppress", bag))
                    {
                        if (!DiagnosticCodes.Exists(code))
                            bag.Add(DiagnosticCodes.SuppressInvalid, file, $"diagnostics.suppress names '{code}', which is not a twllink diagnostic code");
                        else if (DiagnosticCodes.SeverityOf(code) == Severity.Error)
                            bag.Add(DiagnosticCodes.SuppressInvalid, file, $"diagnostics.suppress names '{code}', an error - errors cannot be suppressed");
                        else
                            suppress.Add(code);
                    }
                }

                if (id == null || name == null || author == null || description == null || version == null || api == null
                    || bag.HasErrors)
                    return null;

                return new Manifest
                {
                    Id = id,
                    Name = name,
                    Author = author,
                    Version = version,
                    Description = description,
                    Api = api.Value,
                    Comments = comments,
                    Embed = embed,
                    WarningsAsErrors = warningsAsErrors,
                    Suppress = suppress,
                };
            }
        }

        public static bool IsValidId(string id)
        {
            return id.Length is >= 1 and <= 64 && id.All(c => c is >= 'a' and <= 'z' or >= '0' and <= '9' or '_');
        }

        /// <summary>Strict MAJOR.MINOR.PATCH: exactly three parts, digits only, no leading zeros, no suffixes.</summary>
        public static bool IsValidVersion(string version)
        {
            var parts = version.Split('.');
            return parts.Length == 3 && parts.All(p =>
                p.Length is >= 1 and <= 9 && p.All(char.IsAsciiDigit) && (p.Length == 1 || p[0] != '0'));
        }

        private static void ReportUnknown(JsonElement obj, HashSet<string> known, string prefix, DiagnosticBag bag)
        {
            foreach (var property in obj.EnumerateObject())
            {
                if (!known.Contains(property.Name))
                    bag.Add(DiagnosticCodes.ManifestUnknownField, Manifest.FileName,
                        $"unknown field '{prefix}{property.Name}' is ignored - a typo, or a newer manifest than this twllink knows");
            }
        }

        private static JsonElement? Section(JsonElement root, string name, DiagnosticBag bag)
        {
            if (!root.TryGetProperty(name, out var value))
                return null;
            if (value.ValueKind == JsonValueKind.Object)
                return value;

            bag.Add(DiagnosticCodes.ManifestFieldType, Manifest.FileName, $"'{name}' must be an object");
            return null;
        }

        private static string? RequiredString(JsonElement root, string name, DiagnosticBag bag)
        {
            if (!root.TryGetProperty(name, out var value))
            {
                bag.Add(DiagnosticCodes.ManifestFieldMissing, Manifest.FileName, $"required field '{name}' is missing");
                return null;
            }
            if (value.ValueKind != JsonValueKind.String)
            {
                bag.Add(DiagnosticCodes.ManifestFieldType, Manifest.FileName, $"'{name}' must be a string");
                return null;
            }
            return value.GetString()!;
        }

        /// <summary>
        /// A string that ends up on one line of the bundle's header: the plugin reads the header line by line,
        /// so a line break would end the field early and turn the rest into a line of its own.
        /// </summary>
        private static string? RequiredText(JsonElement root, string name, DiagnosticBag bag)
        {
            var value = RequiredString(root, name, bag);
            if (value == null)
                return null;

            if (value.Trim().Length == 0)
            {
                bag.Add(DiagnosticCodes.ManifestText, Manifest.FileName, $"'{name}' must not be empty");
                return null;
            }
            if (value.Any(char.IsControl))
            {
                bag.Add(DiagnosticCodes.ManifestText, Manifest.FileName,
                    $"'{name}' must be a single line without control characters - it is written into the script's header");
                return null;
            }
            return value.Trim();
        }

        private static int? RequiredInt(JsonElement root, string name, DiagnosticBag bag)
        {
            if (!root.TryGetProperty(name, out var value))
            {
                bag.Add(DiagnosticCodes.ManifestFieldMissing, Manifest.FileName, $"required field '{name}' is missing");
                return null;
            }
            if (value.ValueKind != JsonValueKind.Number || !value.TryGetInt32(out var number))
            {
                bag.Add(DiagnosticCodes.ManifestFieldType, Manifest.FileName, $"'{name}' must be a whole number");
                return null;
            }
            return number;
        }

        private static CommentMode ReadComments(JsonElement build, DiagnosticBag bag)
        {
            if (!build.TryGetProperty("comments", out var value))
                return CommentMode.Keep;

            switch (value.ValueKind == JsonValueKind.String ? value.GetString() : null)
            {
                case "keep":
                    return CommentMode.Keep;
                case "strip":
                    return CommentMode.Strip;
                default:
                    bag.Add(DiagnosticCodes.ManifestComments, Manifest.FileName, "build.comments must be \"keep\" or \"strip\"");
                    return CommentMode.Keep;
            }
        }

        private static List<string> ReadStringArray(JsonElement obj, string name, string display, DiagnosticBag bag)
        {
            var result = new List<string>();
            if (!obj.TryGetProperty(name, out var value))
                return result;

            if (value.ValueKind != JsonValueKind.Array)
            {
                bag.Add(DiagnosticCodes.ManifestFieldType, Manifest.FileName, $"'{display}' must be an array of strings");
                return result;
            }

            foreach (var item in value.EnumerateArray())
            {
                if (item.ValueKind == JsonValueKind.String)
                    result.Add(item.GetString()!);
                else
                    bag.Add(DiagnosticCodes.ManifestFieldType, Manifest.FileName, $"'{display}' must contain only strings");
            }
            return result;
        }

        /// <summary>System.Text.Json appends "Path: $ | LineNumber: ..." - the position is reported separately.</summary>
        private static string FirstSentence(string message)
        {
            var cut = message.IndexOf(" Path:", StringComparison.Ordinal);
            return cut > 0 ? message[..cut] : message;
        }
    }
}
