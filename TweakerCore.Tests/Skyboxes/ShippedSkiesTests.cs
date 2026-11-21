using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

using NUnit.Framework;

namespace TweakerCore.Tests.Skyboxes
{
    /// <summary>
    /// The sky packages that ship with the bundle are the .sky directories of the Skyboxes submodule.
    /// The plugin resolves every path in Config.json inside the package, so a manifest that names a
    /// file the package does not carry is a sky that fails to load in the game - this catches it in
    /// the test run instead.
    /// </summary>
    [TestFixture]
    public class ShippedSkiesTests
    {
        private static readonly JsonDocumentOptions Options = new()
        {
            CommentHandling = JsonCommentHandling.Skip,
            AllowTrailingCommas = true,
        };

        private static IEnumerable<TestCaseData> Packages()
        {
            var root = RepositoryRoot();
            var folder = Path.Combine(root, "Skyboxes");
            if (!Directory.Exists(folder))
                yield break;

            foreach (var package in Directory.EnumerateDirectories(folder, "*.sky").OrderBy(p => p))
                yield return new TestCaseData(package).SetName("ManifestFilesExist(" + Path.GetFileName(package) + ")");
        }

        [Test]
        public void AtLeastOnePackageShips()
        {
            Assert.That(Packages(), Is.Not.Empty,
                "no .sky package under Skyboxes - initialize the submodule: git submodule update --init --recursive");
        }

        [TestCaseSource(nameof(Packages))]
        public void ManifestFilesExist(string packageDirectory)
        {
            var manifest = Path.Combine(packageDirectory, "Config.json");
            Assert.That(File.Exists(manifest), Is.True, $"{Path.GetFileName(packageDirectory)} has no Config.json");

            using var document = JsonDocument.Parse(File.ReadAllText(manifest), Options);
            var layers = document.RootElement.GetProperty("layers");
            Assert.That(layers.GetArrayLength(), Is.GreaterThan(0), "a package has at least one layer");

            foreach (var layer in layers.EnumerateArray())
            {
                var kind = layer.GetProperty("kind").GetString();

                if (layer.TryGetProperty("shader", out var shaderElement))
                {
                    var shader = shaderElement.GetString()!;
                    // A sprite layer names the pair by its stem (Docs/skyboxes/clouds.md).
                    if (string.Equals(kind, "sprites", StringComparison.Ordinal))
                    {
                        AssertFile(packageDirectory, shader + ".vs.hlsl");
                        AssertFile(packageDirectory, shader + ".ps.hlsl");
                    }
                    else
                    {
                        AssertFile(packageDirectory, shader);
                    }
                }

                if (layer.TryGetProperty("textures", out var textures))
                {
                    foreach (var texture in textures.EnumerateArray())
                        AssertFile(packageDirectory, texture.GetProperty("path").GetString()!);
                }

                if (layer.TryGetProperty("generator", out var generator))
                    AssertFile(packageDirectory, generator.GetString()!);

                if (layer.TryGetProperty("fill", out var fill) && fill.TryGetProperty("generator", out var fillGenerator))
                    AssertFile(packageDirectory, fillGenerator.GetString()!);
            }
        }

        private static void AssertFile(string packageDirectory, string relativePath)
        {
            var full = Path.Combine(packageDirectory, relativePath.Replace('/', Path.DirectorySeparatorChar));
            Assert.That(File.Exists(full), Is.True,
                $"{Path.GetFileName(packageDirectory)}: the manifest names {relativePath}, which the package does not carry");
        }

        private static string RepositoryRoot()
        {
            for (var dir = new DirectoryInfo(TestContext.CurrentContext.TestDirectory); dir != null; dir = dir.Parent)
            {
                if (Directory.Exists(Path.Combine(dir.FullName, "TweakerUI")) &&
                    Directory.Exists(Path.Combine(dir.FullName, "TweakerPlugin")))
                {
                    return dir.FullName;
                }
            }

            throw new AssertionException("repository root not found from " + TestContext.CurrentContext.TestDirectory);
        }
    }
}
