using System.Collections.Generic;

namespace TweakerLuaLinker.Diagnostics
{
    /// <summary>
    /// Every diagnostic the linker can produce. Codes are numbered once and never reused
    /// (lua-engine-fix-roadmap.md §9.7): TweakerUI builds hints from them and the documentation links to
    /// them, so a retired code stays retired rather than coming back meaning something else.
    ///
    ///   TW00xx   the manifest (twproject.json) and the files it names
    ///   TW01xx   sources and the require graph
    ///   TW02xx   producing the output
    ///   TW03xx   what the code reaches for: the sandbox, tw.*, globals (checked against api.json)
    ///
    /// Docs/scripting/authoring.md lists them for script authors; a new code goes there too.
    /// </summary>
    internal static class DiagnosticCodes
    {
        public const string ManifestMissing = "TW0001";
        public const string ManifestSyntax = "TW0002";
        public const string ManifestFormat = "TW0003";
        public const string ManifestFieldMissing = "TW0004";
        public const string ManifestFieldType = "TW0005";
        public const string ManifestId = "TW0006";
        public const string ManifestVersion = "TW0007";
        public const string ManifestText = "TW0008";
        public const string ManifestApi = "TW0009";
        public const string ManifestUnknownField = "TW0010";
        public const string ManifestComments = "TW0011";
        public const string EmbedMissing = "TW0012";
        public const string EmbedOutsideProject = "TW0013";
        public const string EmbedNotText = "TW0014";
        public const string SuppressInvalid = "TW0015";
        public const string ApiTooNew = "TW0016";

        public const string MainMissing = "TW0101";
        public const string SyntaxError = "TW0102";
        public const string DynamicRequire = "TW0103";
        public const string ModuleNotFound = "TW0104";
        public const string ModuleNameInvalid = "TW0105";
        public const string ModuleNameCase = "TW0106";
        public const string RequireCycle = "TW0107";
        public const string RequireMain = "TW0108";
        public const string UnreachableModule = "TW0109";
        public const string SourceUnreadable = "TW0110";

        public const string OutputWriteFailed = "TW0201";
        public const string BundleInvalid = "TW0202";
        public const string InstallFailed = "TW0203";

        public const string SandboxRemoved = "TW0301";
        public const string UnknownTwName = "TW0302";
        public const string NewerThanTarget = "TW0303";
        public const string WritesTw = "TW0304";
        public const string GlobalAssigned = "TW0305";
        public const string UnknownGlobal = "TW0306";
        public const string UnknownLibraryName = "TW0307";

        private static readonly HashSet<string> _warnings = new()
        {
            ManifestUnknownField,
            SuppressInvalid,
            UnreachableModule,
            NewerThanTarget,
            GlobalAssigned,
            UnknownGlobal,
            UnknownLibraryName,
        };

        private static readonly HashSet<string> _all = new()
        {
            ManifestMissing, ManifestSyntax, ManifestFormat, ManifestFieldMissing, ManifestFieldType, ManifestId,
            ManifestVersion, ManifestText, ManifestApi, ManifestUnknownField, ManifestComments, EmbedMissing,
            EmbedOutsideProject, EmbedNotText, SuppressInvalid, ApiTooNew,
            MainMissing, SyntaxError, DynamicRequire, ModuleNotFound, ModuleNameInvalid, ModuleNameCase,
            RequireCycle, RequireMain, UnreachableModule, SourceUnreadable,
            OutputWriteFailed, BundleInvalid, InstallFailed,
            SandboxRemoved, UnknownTwName, NewerThanTarget, WritesTw, GlobalAssigned, UnknownGlobal, UnknownLibraryName,
        };

        public static IReadOnlyCollection<string> All => _all;

        public static bool Exists(string code)
        {
            return _all.Contains(code);
        }

        /// <summary>A code's severity is part of its definition, not something each call site decides.</summary>
        public static Severity SeverityOf(string code)
        {
            return _warnings.Contains(code) ? Severity.Warning : Severity.Error;
        }
    }
}
