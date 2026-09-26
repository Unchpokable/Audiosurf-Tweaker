using System;
using System.Collections.Generic;
using System.Linq;

using Loretta.CodeAnalysis;
using Loretta.CodeAnalysis.Lua;
using Loretta.CodeAnalysis.Lua.Syntax;

using TweakerLuaLinker.Api;
using TweakerLuaLinker.Diagnostics;

namespace TweakerLuaLinker.Linking
{
    /// <summary>
    /// What the project's code reaches for, checked against what the plugin's environment has
    /// (Docs/scripting/api.json; lua-engine-fix-roadmap.md §9.3, §9.8). The point is that the sandbox and a
    /// misspelt tw.* name stop being runtime surprises in the game and become build errors with a file and
    /// a line.
    ///
    /// Only globals are looked at - a name a module declares with `local` is its own business - and
    /// only what can be known without running anything: a chain of plain field accesses off a global
    /// (tw.hud.text, os.execute). A handle's methods would need types, and a table indexed with a
    /// computed key has no name to check.
    /// </summary>
    internal sealed class ApiChecker
    {
        private const string Tw = "tw";

        private readonly ApiDescription _api;
        private readonly int _targetApi;
        private readonly DiagnosticBag _bag;

        // The script's environment is shared by every module (it is one script once bundled), so a
        // global one module assigns is a known name to all the others.
        private readonly HashSet<string> _assigned = new(StringComparer.Ordinal);

        // Once per name for the project-wide warnings: fifty uses of one unknown global are one mistake.
        private readonly HashSet<string> _reported = new(StringComparer.Ordinal);

        private ApiChecker(ApiDescription api, int targetApi, DiagnosticBag bag)
        {
            _api = api;
            _targetApi = targetApi;
            _bag = bag;
        }

        public static void Check(IReadOnlyList<Module> modules, ApiDescription api, int targetApi, DiagnosticBag bag)
        {
            var checker = new ApiChecker(api, targetApi, bag);
            var parsed = modules.Where(m => m.Parsed).ToList();

            foreach (var module in parsed)
                checker.CollectAssignments(module);
            foreach (var module in parsed)
                checker.CheckModule(module);
        }

        // --- the two passes -----------------------------------------------------------------------------

        private void CollectAssignments(Module module)
        {
            foreach (var use in Uses(module))
            {
                if (use.Write && use.Chain.Count == 1)
                    _assigned.Add(use.Chain[0]);
            }
        }

        private void CheckModule(Module module)
        {
            foreach (var use in Uses(module))
            {
                var root = use.Chain[0];

                // The bundle's own require, a local of the script; the graph has checked its uses.
                if (root == "require")
                    continue;

                if (use.Write)
                {
                    CheckWrite(module, use);
                    continue;
                }

                if (_api.Removed.Contains(root))
                {
                    Report(DiagnosticCodes.SandboxRemoved, module, use.Node,
                        $"'{root}' is not available to scripts - the plugin removes it (Docs/scripting/limits.md, \"What is not in the environment\")");
                    continue;
                }

                if (!_api.Globals.ContainsKey(root))
                {
                    if (!_assigned.Contains(root) && _reported.Add("global:" + root))
                    {
                        Report(DiagnosticCodes.UnknownGlobal, module, use.Node,
                            $"'{root}' is not a global of the script environment and nothing in the project assigns it - a typo, or a missing 'local'?");
                    }
                    continue;
                }

                CheckFields(module, use);
            }
        }

        private void CheckWrite(Module module, Use use)
        {
            var root = use.Chain[0];
            if (root == Tw)
            {
                var what = use.Chain.Count == 1 ? "replaces tw for this script" : $"writes '{string.Join('.', use.Chain)}' into the tw every script shares";
                Report(DiagnosticCodes.WritesTw, module, use.Node, $"this {what} - keep your own values in locals or in a table of your own");
                return;
            }

            // A field of some other table (`string.pad = ...`, `M.x = ...` on a global M) is left alone:
            // extending a table is ordinary Lua, and only tw is shared with every other script.
            if (use.Chain.Count == 1 && _reported.Add("assigned:" + root))
            {
                Report(DiagnosticCodes.GlobalAssigned, module, use.Node,
                    $"'{root}' is assigned without 'local', so it is a global - shared by every module of the script. Use 'local' unless that is the point");
            }
        }

        /// <summary>Walks tw.hud.text one field at a time for as long as the description knows the table.</summary>
        private void CheckFields(Module module, Use use)
        {
            var path = use.Chain[0];
            for (var i = 1; i < use.Chain.Count; i++)
            {
                if (!_api.Globals.TryGetValue(path, out var parent) || !parent.IsTable || path == "_G")
                    return;

                var field = use.Chain[i];
                var next = path + "." + field;

                if (_api.Removed.Contains(next))
                {
                    Report(DiagnosticCodes.SandboxRemoved, module, use.Node,
                        $"'{next}' is not available to scripts - the plugin removes it (Docs/scripting/limits.md, \"What is not in the environment\")");
                    return;
                }

                if (!_api.Globals.TryGetValue(next, out var entry))
                {
                    var hint = Suggestion(path, field);
                    if (use.Chain[0] == Tw)
                    {
                        Report(DiagnosticCodes.UnknownTwName, module, use.Node,
                            $"'{next}' does not exist in API {_api.Api}{hint}");
                    }
                    else if (_reported.Add("field:" + next))
                    {
                        Report(DiagnosticCodes.UnknownLibraryName, module, use.Node,
                            $"'{next}' does not exist in the scripts' Lua (LuaJIT 2.1 as the plugin sets it up) and reads as nil{hint}");
                    }
                    return;
                }

                if (use.Chain[0] == Tw && entry.Since > _targetApi)
                {
                    Report(DiagnosticCodes.NewerThanTarget, module, use.Node,
                        $"'{next}' is API {entry.Since}, and the project targets API {_targetApi} - raise \"api\" in twproject.json, or a plugin that has API {_targetApi} will not have it");
                }
                path = next;
            }
        }

        private string Suggestion(string table, string field)
        {
            var best = _api.Fields(table)
                .Select(name => (name, distance: Distance(name, field)))
                .Where(c => c.distance <= Math.Max(1, field.Length / 3))
                .OrderBy(c => c.distance)
                .ThenBy(c => c.name, StringComparer.Ordinal)
                .FirstOrDefault();
            return best.name == null ? "" : $" - did you mean '{table}.{best.name}'?";
        }

        private static int Distance(string a, string b)
        {
            var row = Enumerable.Range(0, b.Length + 1).ToArray();
            for (var i = 1; i <= a.Length; i++)
            {
                var diagonal = row[0];
                row[0] = i;
                for (var j = 1; j <= b.Length; j++)
                {
                    var above = row[j];
                    row[j] = Math.Min(Math.Min(row[j] + 1, row[j - 1] + 1), diagonal + (a[i - 1] == b[j - 1] ? 0 : 1));
                    diagonal = above;
                }
            }
            return row[b.Length];
        }

        private void Report(string code, Module module, SyntaxNode node, string message)
        {
            var position = node.GetLocation().GetLineSpan().StartLinePosition;
            _bag.Add(code, module.ProjectPath, position.Line + 1, position.Character + 1, message);
        }

        // --- finding the uses -------------------------------------------------------------------------

        /// <summary>A global and the plain field accesses hanging off it: `tw.hud.text` is [tw, hud, text].</summary>
        private sealed record Use(IReadOnlyList<string> Chain, bool Write, SyntaxNode Node);

        private static IEnumerable<Use> Uses(Module module)
        {
            var script = module.Script;
            foreach (var node in module.Tree.GetRoot().DescendantNodes())
            {
                switch (node)
                {
                    case IdentifierNameSyntax identifier when IsGlobal(script, identifier):
                    {
                        var chain = new List<string> { identifier.Name };
                        ExpressionSyntax top = identifier;
                        while (top.Parent is MemberAccessExpressionSyntax member && member.Expression == top)
                        {
                            chain.Add(member.MemberName.Text);
                            top = member;
                        }
                        var write = top.Parent is AssignmentStatementSyntax assignment && assignment.Variables.Contains(top);
                        yield return new Use(chain, write, identifier);
                        break;
                    }
                    case FunctionDeclarationStatementSyntax declaration:
                    {
                        // `function name()` and `function a.b.c()` / `function a.b:c()` - assignments
                        // that are not written with '='.
                        var chain = new List<string>();
                        var name = declaration.Name;
                        while (true)
                        {
                            if (name is MemberFunctionNameSyntax member)
                            {
                                chain.Insert(0, member.Name.Text);
                                name = member.BaseName;
                            }
                            else if (name is MethodFunctionNameSyntax method)
                            {
                                chain.Insert(0, method.Name.Text);
                                name = method.BaseName;
                            }
                            else
                            {
                                break;
                            }
                        }
                        if (name is SimpleFunctionNameSyntax simple && script.GetVariable(simple) is null or { Kind: VariableKind.Global })
                        {
                            chain.Insert(0, simple.Name.Text);
                            yield return new Use(chain, true, simple);
                        }
                        break;
                    }
                }
            }
        }

        private static bool IsGlobal(Script script, IdentifierNameSyntax identifier)
        {
            return script.GetVariable(identifier) is { Kind: VariableKind.Global };
        }
    }
}
