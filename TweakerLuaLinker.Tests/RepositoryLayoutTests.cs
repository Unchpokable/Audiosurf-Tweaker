using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

using NUnit.Framework;

namespace TweakerLuaLinker.Tests
{
    /// <summary>
    /// The components the deploy needs are submodules (Docs/Internal/repo-layout-plan.md §3). A clone
    /// that did not initialize them should fail with the command to run, not by quietly shipping a
    /// bundle without a sky or without the script it was supposed to build.
    /// </summary>
    [TestFixture]
    public class RepositoryLayoutTests
    {
        [TestCase("ScriptProjects/puzzlepro_hud/twproject.json")]
        [TestCase("Skyboxes/Canvas.sky/Config.json")]
        public void ComponentSubmoduleIsInitialized(string relativePath)
        {
            var root = LuaRunner.RepositoryRoot();
            Assert.That(root, Is.Not.Null, "repository root not found");
            var full = Path.Combine(root!, relativePath.Replace('/', Path.DirectorySeparatorChar));
            Assert.That(File.Exists(full), Is.True,
                $"{relativePath} is missing - run: git submodule update --init --recursive");
        }
    }
}
