using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.IO;
using System.Linq;
using System.Text;

using Loretta.CodeAnalysis;
using Loretta.CodeAnalysis.Lua;
using Loretta.CodeAnalysis.Lua.Syntax;
using Loretta.CodeAnalysis.Text;

using TweakerLuaLinker.Diagnostics;

namespace TweakerLuaLinker.Linking
{
    /// <summary>One `require("x")` in a module, with where it was written.</summary>
    internal sealed record RequireSite(string Name, int Line, int Column);

    /// <summary>A source file reached from main, parsed.</summary>
    internal sealed class Module
    {
        /// <summary>The name `require` uses: "main", "palette", "draw.rail".</summary>
        public required string Name { get; init; }

        /// <summary>Project-relative, forward slashes: "src/draw/rail.lua". What diagnostics point at.</summary>
        public required string ProjectPath { get; init; }

        /// <summary>
        /// Relative to src/: "draw/rail.lua". What the line map names, and so what the plugin shows in an
        /// error - the shortest path that is still unique within the project.
        /// </summary>
        public required string SourcePath { get; init; }

        public required SourceText Text { get; init; }
        public required SyntaxTree Tree { get; init; }
        public List<RequireSite> Requires { get; } = new();

        /// <summary>Scope analysis of the module, built once and shared by every check that needs it.</summary>
        public Script Script => _script ??= new Script(ImmutableArray.Create(Tree));

        /// <summary>False when the module has syntax errors - nothing past parsing looks at it then.</summary>
        public bool Parsed { get; init; }

        private Script? _script;
    }

    /// <summary>
    /// The project as the require graph sees it (lua-engine-fix-roadmap.md §9.4): src/main.lua, and every
    /// file reachable from it through require calls with a literal name - nothing else. There is no list of
    /// sources to keep in sync; a file nobody requires is not part of the build, and is reported so that a
    /// misspelt module name does not go unnoticed.
    /// </summary>
    internal sealed class ModuleGraph
    {
        public const string SourceFolder = "src";
        public const string MainModule = "main";

        private static readonly LuaParseOptions _parseOptions = new(LuaSyntaxOptions.LuaJIT21);

        private readonly string _root;
        private readonly string _sourceRoot;
        private readonly DiagnosticBag _bag;
        private readonly Dictionary<string, Module> _modules = new(StringComparer.Ordinal);
        private readonly List<Module> _loaded = new();

        /// <summary>Every module that was read, in the order it was reached - whether the graph built or not.</summary>
        public IReadOnlyList<Module> Loaded => _loaded;

        /// <summary>Dependencies before dependents, main last. Empty when the graph has errors.</summary>
        public IReadOnlyList<Module> Order { get; private set; } = [];

        private ModuleGraph(string root, DiagnosticBag bag)
        {
            _root = root;
            _sourceRoot = Path.Combine(root, SourceFolder);
            _bag = bag;
        }

        public static LuaParseOptions ParseOptions => _parseOptions;

        public static ModuleGraph Build(string projectRoot, DiagnosticBag bag)
        {
            var graph = new ModuleGraph(projectRoot, bag);
            graph.Discover();
            return graph;
        }

        private void Discover()
        {
            var mainPath = Path.Combine(_sourceRoot, MainModule + ".lua");
            if (!File.Exists(mainPath))
            {
                _bag.Add(DiagnosticCodes.MainMissing, SourceFolder + "/" + MainModule + ".lua",
                    "the entry point src/main.lua does not exist - every project starts there");
                return;
            }

            var pending = new Queue<(string Name, string FullPath)>();
            pending.Enqueue((MainModule, mainPath));
            var seen = new HashSet<string>(StringComparer.Ordinal) { MainModule };

            while (pending.Count > 0)
            {
                var (name, fullPath) = pending.Dequeue();
                var module = Load(name, fullPath);
                if (module == null)
                    continue;

                _modules.Add(name, module);
                _loaded.Add(module);
                foreach (var site in module.Requires)
                {
                    var target = Resolve(module, site);
                    if (target != null && seen.Add(site.Name))
                        pending.Enqueue((site.Name, target));
                }
            }

            ReportUnreachable();

            if (_bag.HasErrors)
                return;

            Order = TopologicalOrder();
        }

        private Module? Load(string name, string fullPath)
        {
            var sourcePath = Path.GetRelativePath(_sourceRoot, fullPath).Replace('\\', '/');
            var projectPath = SourceFolder + "/" + sourcePath;

            string text;
            try
            {
                text = ReadSource(fullPath);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException or DecoderFallbackException)
            {
                _bag.Add(DiagnosticCodes.SourceUnreadable, projectPath, $"cannot read the module: {e.Message}");
                return null;
            }

            var tree = LuaSyntaxTree.ParseText(text, _parseOptions, projectPath);
            var syntaxErrors = tree.GetDiagnostics().Where(d => d.Severity == DiagnosticSeverity.Error).ToList();
            var module = new Module
            {
                Name = name,
                ProjectPath = projectPath,
                SourcePath = sourcePath,
                Text = tree.GetText(),
                Tree = tree,
                Parsed = syntaxErrors.Count == 0,
            };

            foreach (var error in syntaxErrors)
            {
                var span = error.Location.GetLineSpan().StartLinePosition;
                _bag.Add(DiagnosticCodes.SyntaxError, projectPath, span.Line + 1, span.Character + 1, $"{error.GetMessage()} ({error.Id})");
            }

            // The require list of a file that does not parse is a guess, and a wrong guess produces errors
            // about modules that were never asked for. The syntax errors are what the author has to fix first.
            if (syntaxErrors.Count == 0)
                CollectRequires(module);

            return module;
        }

        /// <summary>UTF-8, strictly: a mis-encoded module is an error here rather than mojibake in the game.</summary>
        private static string ReadSource(string path)
        {
            var bytes = File.ReadAllBytes(path);
            var start = bytes.Length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF ? 3 : 0;
            return new UTF8Encoding(false, true).GetString(bytes, start, bytes.Length - start);
        }

        private void CollectRequires(Module module)
        {
            var script = module.Script;
            foreach (var call in module.Tree.GetRoot().DescendantNodes().OfType<FunctionCallExpressionSyntax>())
            {
                if (call.Expression is not IdentifierNameSyntax { Name: "require" } identifier)
                    continue;

                // Only the global require is ours. A module that declares `local require = ...` has its own,
                // and a call to it is none of the linker's business.
                if (script.GetVariable(identifier) is { Kind: not VariableKind.Global })
                    continue;

                var position = call.GetLocation().GetLineSpan().StartLinePosition;
                var argument = call.Argument switch
                {
                    StringFunctionArgumentSyntax s => s.Expression,
                    ExpressionListFunctionArgumentSyntax { Expressions.Count: 1 } list => list.Expressions[0],
                    _ => null,
                };

                if (argument is LiteralExpressionSyntax { Token.Value: string name })
                {
                    module.Requires.Add(new RequireSite(name, position.Line + 1, position.Character + 1));
                }
                else
                {
                    _bag.Add(DiagnosticCodes.DynamicRequire, module.ProjectPath, position.Line + 1, position.Character + 1,
                        "require takes exactly one string literal - the linker has to know every module at build time");
                }
            }
        }

        /// <summary>The file a require names, or null with a diagnostic.</summary>
        private string? Resolve(Module from, RequireSite site)
        {
            if (site.Name == MainModule)
            {
                _bag.Add(DiagnosticCodes.RequireMain, from.ProjectPath, site.Line, site.Column,
                    "main is the entry point and cannot be required - move what is shared into a module of its own");
                return null;
            }

            var segments = site.Name.Split('.');
            if (segments.Any(s => s.Length == 0 || !s.All(c => char.IsAsciiLetterOrDigit(c) || c == '_')))
            {
                _bag.Add(DiagnosticCodes.ModuleNameInvalid, from.ProjectPath, site.Line, site.Column,
                    $"'{site.Name}' is not a module name - use dot-separated parts of letters, digits and '_', e.g. \"draw.rail\"");
                return null;
            }

            // Windows would happily open palette.lua for require("Palette"). Then the bundle holds the
            // module under two names and runs it twice, with two copies of its state - so the case must match.
            var directory = _sourceRoot;
            for (var i = 0; i < segments.Length; i++)
            {
                var last = i == segments.Length - 1;
                var wanted = last ? segments[i] + ".lua" : segments[i];
                var actual = FindEntry(directory, wanted, last);

                var expected = SourceFolder + "/" + string.Join('/', segments) + ".lua";
                if (actual == null)
                {
                    _bag.Add(DiagnosticCodes.ModuleNotFound, from.ProjectPath, site.Line, site.Column,
                        $"module '{site.Name}' not found - expected the file {expected}");
                    return null;
                }
                if (!string.Equals(actual, wanted, StringComparison.Ordinal))
                {
                    _bag.Add(DiagnosticCodes.ModuleNameCase, from.ProjectPath, site.Line, site.Column,
                        $"module '{site.Name}' differs in case from the file on disk ('{actual}') - module names are case-sensitive");
                    return null;
                }
                directory = Path.Combine(directory, actual);
            }
            return directory;
        }

        private static string? FindEntry(string directory, string name, bool file)
        {
            if (!Directory.Exists(directory))
                return null;

            var entries = file ? Directory.EnumerateFiles(directory) : Directory.EnumerateDirectories(directory);
            return entries.Select(Path.GetFileName).FirstOrDefault(n => string.Equals(n, name, StringComparison.OrdinalIgnoreCase));
        }

        private void ReportUnreachable()
        {
            if (!Directory.Exists(_sourceRoot))
                return;

            var reached = new HashSet<string>(_modules.Values.Select(m => m.SourcePath), StringComparer.OrdinalIgnoreCase);
            var files = Directory.EnumerateFiles(_sourceRoot, "*.lua", SearchOption.AllDirectories)
                .Select(f => Path.GetRelativePath(_sourceRoot, f).Replace('\\', '/'))
                .Where(f => !reached.Contains(f))
                .OrderBy(f => f, StringComparer.Ordinal);

            foreach (var file in files)
            {
                var name = file[..^".lua".Length].Replace('/', '.');
                _bag.Add(DiagnosticCodes.UnreachableModule, SourceFolder + "/" + file,
                    $"not part of the build: nothing reachable from main requires '{name}'");
            }
        }

        /// <summary>
        /// Depth-first from main, dependencies emitted before their dependents, siblings in the order they are
        /// required - so the bundle reads top-down and is the same on every build. Any cycle is an error: the
        /// bundle's require would hand the second caller a half-loaded module (§9.5).
        /// </summary>
        private IReadOnlyList<Module> TopologicalOrder()
        {
            var order = new List<Module>();
            var state = new Dictionary<string, bool>(StringComparer.Ordinal); // false: on the stack, true: done
            var stack = new List<string>();
            var cyclic = false;

            void Visit(Module module)
            {
                state[module.Name] = false;
                stack.Add(module.Name);
                foreach (var site in module.Requires)
                {
                    if (!_modules.TryGetValue(site.Name, out var dependency))
                        continue;

                    if (!state.TryGetValue(site.Name, out var done))
                    {
                        Visit(dependency);
                    }
                    else if (!done)
                    {
                        var loop = stack.Skip(stack.IndexOf(site.Name)).Append(site.Name);
                        _bag.Add(DiagnosticCodes.RequireCycle, module.ProjectPath, site.Line, site.Column,
                            $"require cycle: {string.Join(" -> ", loop)} - a module cannot (even indirectly) require itself");
                        cyclic = true;
                    }
                }
                stack.RemoveAt(stack.Count - 1);
                state[module.Name] = true;
                order.Add(module);
            }

            Visit(_modules[MainModule]);
            return cyclic ? [] : order;
        }
    }
}
