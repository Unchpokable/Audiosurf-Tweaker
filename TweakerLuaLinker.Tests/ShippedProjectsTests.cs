using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

using NUnit.Framework;

using TweakerLuaLinker.Linking;

using static TweakerLuaLinker.Tests.Assertions;

namespace TweakerLuaLinker.Tests
{
    /// <summary>
    /// The scripts the Tweaker ships that are built from a project (the ScriptProjects submodule) are
    /// committed as their build output, next to the hand-written ones in assets/scripts/. That keeps
    /// the deploy free of a linker step - and makes it possible for the two to drift apart: a module
    /// edited, the bundle not rebuilt. This is the check that they have not.
    /// </summary>
    [TestFixture]
    public class ShippedProjectsTests
    {
        private static IEnumerable<TestCaseData> Projects()
        {
            var root = LuaRunner.RepositoryRoot();
            if (root == null)
                yield break;

            var folder = Path.Combine(root, "ScriptProjects");
            if (!Directory.Exists(folder))
                yield break;

            foreach (var manifest in Directory.EnumerateFiles(folder, "twproject.json", SearchOption.AllDirectories)
                                               .Where(p => !IsUnderDev(folder, p))
                                               .OrderBy(p => p))
            {
                var project = Path.GetDirectoryName(manifest)!;
                yield return new TestCaseData(project).SetName("ShippedBundleIsUpToDate(" + Path.GetFileName(project) + ")");
            }
        }

        [Test]
        public void ThereIsAtLeastOneProject()
        {
            Assert.That(Projects(), Is.Not.Empty,
                "puzzlepro_hud is built from a project in the ScriptProjects submodule; finding none means the submodule is not initialized (git submodule update --init --recursive) or the lookup is broken");
        }

        [TestCaseSource(nameof(Projects))]
        public void ShippedBundleIsUpToDate(string project)
        {
            var result = Linker.Link(project);
            Succeeded(result);
            Assert.That(result.Diagnostics, Is.Empty, "a shipped project builds without warnings:\n" + Describe(result.Diagnostics));

            // ScriptProjects/<sub>/<project>/ builds into assets/scripts/<sub>/. dev/ is ignored by the
            // submodule's .gitignore and never ships; a project under it is not a shipped one.
            var root = LuaRunner.RepositoryRoot()!;
            var projects = Path.Combine(root, "ScriptProjects");
            var sub = Path.GetDirectoryName(Path.GetRelativePath(projects, project)) ?? "";
            var scripts = Path.Combine(root, "TweakerPlugin", "assets", "scripts", sub);
            var bundle = Path.GetFullPath(Path.Combine(scripts, result.Manifest!.OutputFileName));
            var fix = $"rebuild it: twllink --project {Relative(project)} --out {Relative(bundle)}";

            Assert.That(File.Exists(bundle), Is.True, $"{Relative(bundle)} is missing - {fix}");
            Assert.That(Lf(File.ReadAllText(bundle)), Is.EqualTo(result.Bundle), $"{Relative(bundle)} is not what its project builds - {fix}");
            Assert.That(Lf(File.ReadAllText(bundle + ".map")), Is.EqualTo(result.Map), $"{Relative(bundle)}.map is stale - {fix}");
        }

        private static bool IsUnderDev(string projects, string manifest)
        {
            return Path.GetRelativePath(projects, manifest)
                       .StartsWith("dev" + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase);
        }

        // git may check either file out with CRLF; the plugin ignores carriage returns for the same reason.
        private static string Lf(string text)
        {
            return text.Replace("\r\n", "\n");
        }

        private static string Relative(string path)
        {
            return Path.GetRelativePath(LuaRunner.RepositoryRoot()!, path).Replace('\\', '/');
        }
    }
}
