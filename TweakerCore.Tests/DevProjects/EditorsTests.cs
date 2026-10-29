namespace TweakerCore.Tests.DevProjects
{
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.Linq;
    using NUnit.Framework;
    using TweakerCore.DevProjects;

    /// <summary>
    /// Finding editors in fake install folders, and the command lines and environment they are started with.
    /// Nothing is launched: what an editor does with its arguments is the editor's, and the owner checks that live.
    /// </summary>
    [TestFixture]
    public class EditorsTests
    {
        private string _root;
        private EditorSearch _search;

        [SetUp]
        public void SetUp()
        {
            _root = Path.Combine(Path.GetTempPath(), "TweakerCoreTests", "Editors", Guid.NewGuid().ToString("N"));
            _search = new EditorSearch
            {
                LocalAppData = Path.Combine(_root, "Local"),
                ProgramFiles = Path.Combine(_root, "Program Files"),
                ProgramFilesX86 = Path.Combine(_root, "Program Files (x86)"),
                UserProfile = Path.Combine(_root, "User"),
                PathVariable = ""
            };
        }

        [TearDown]
        public void TearDown()
        {
            if (Directory.Exists(_root))
                Directory.Delete(_root, recursive: true);
        }

        private string Touch(params string[] parts)
        {
            var path = Path.Combine(new[] { _root }.Concat(parts).ToArray());
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, "");
            return path;
        }

        [Test]
        public void FindsEditorsInTheirDefaultFoldersInTheListedOrder()
        {
            Touch("Program Files", "Neovim", "bin", "nvim.exe");
            Touch("Local", "Programs", "Microsoft VS Code", "Code.exe");
            Touch("Program Files (x86)", "Notepad++", "notepad++.exe");

            var found = Editors.Detect(_search);

            Assert.That(found.Select(e => e.Id), Is.EqualTo(new[] { "vscode", "notepadpp", "neovim" }));
            Assert.That(found[0].ExecutablePath, Does.EndWith(Path.Combine("Microsoft VS Code", "Code.exe")));
        }

        [Test]
        public void FindsAnEditorThroughItsShimOnPath()
        {
            var cursor = Touch("Elsewhere", "cursor", "Cursor.exe");
            Touch("Elsewhere", "cursor", "resources", "app", "bin", "cursor.cmd");
            _search = new EditorSearch { PathVariable = "C:\\nothing;" + Path.Combine(_root, "Elsewhere", "cursor", "resources", "app", "bin") };

            var editor = Editors.Detect(_search).Single();

            Assert.That(editor.Id, Is.EqualTo("cursor"));
            Assert.That(editor.ExecutablePath, Is.EqualTo(Path.GetFullPath(cursor)));
        }

        [Test]
        public void AShimLeadingNowhereFindsNothing()
        {
            Touch("Stray", "bin", "code.cmd");
            _search = new EditorSearch { PathVariable = Path.Combine(_root, "Stray", "bin") };

            Assert.That(Editors.Detect(_search), Is.Empty);
        }

        [Test]
        public void GitsOwnVimIsNotAnEditor()
        {
            Touch("Git", "git-bash.exe");
            Touch("Git", "usr", "bin", "vim.exe");
            var real = Touch("Tools", "vim", "vim.exe");
            _search = new EditorSearch { PathVariable = Path.Combine(_root, "Git", "usr", "bin") + ";" + Path.Combine(_root, "Tools", "vim") };

            Assert.That(Editors.Detect(_search).Single().ExecutablePath, Is.EqualTo(Path.GetFullPath(real)));
        }

        [Test]
        public void TheNewestVimIsTaken()
        {
            Touch("Program Files", "Vim", "vim90", "vim.exe");
            var newest = Touch("Program Files", "Vim", "vim91", "vim.exe");

            Assert.That(Editors.Detect(_search).Single().ExecutablePath, Is.EqualTo(Path.GetFullPath(newest)));
        }

        [Test]
        public void TheEditorGetsTheToolFoldersFirstOnPathAndTheGameFolder()
        {
            Touch("Local", "Programs", "Microsoft VS Code", "Code.exe");
            var editor = Editors.Detect(_search).Single();

            var tools = new[] { @"C:\Tweaker\DevTools\Binaries", @"C:\Tweaker\DevTools\CgrPy" };
            var info = Editors.StartInfo(editor, @"C:\p\my_hud", null, 0, 0, ScriptDevLike(tools, @"D:\Games\Audiosurf"));

            Assert.That(info.Environment["PATH"],
                Does.StartWith(@"C:\Tweaker\DevTools\Binaries" + Path.PathSeparator + @"C:\Tweaker\DevTools\CgrPy" + Path.PathSeparator),
                "twllink and cgrpy, in that order, ahead of everything the user had");
            Assert.That(info.Environment["Path"], Is.EqualTo(info.Environment["PATH"]), "one variable, whatever its case");
            Assert.That(info.Environment["AUDIOSURF_DIR"], Is.EqualTo(@"D:\Games\Audiosurf"));
            Assert.That(info.UseShellExecute, Is.False, "the environment only reaches a process started without the shell");
            Assert.That(info.ArgumentList, Is.EqualTo(new[] { @"C:\p\my_hud" }));
        }

        [Test]
        public void WithoutAGameFolderAudiosurfDirIsLeftAlone()
        {
            Touch("Local", "Programs", "Microsoft VS Code", "Code.exe");
            var editor = Editors.Detect(_search).Single();
            var inherited = Environment.GetEnvironmentVariable("AUDIOSURF_DIR");

            var info = Editors.StartInfo(editor, @"C:\p", null, 0, 0, ScriptDevLike(new[] { @"C:\bin" }, null));

            Assert.That(info.Environment.TryGetValue("AUDIOSURF_DIR", out var value) ? value : null, Is.EqualTo(inherited));
        }

        [Test]
        public void AnEmptyEnvironmentLeavesTheEditorWithTweakersOwn()
        {
            Touch("Local", "Programs", "Microsoft VS Code", "Code.exe");
            var editor = Editors.Detect(_search).Single();
            var inheritedPath = Environment.GetEnvironmentVariable("PATH");
            var inheritedGame = Environment.GetEnvironmentVariable("AUDIOSURF_DIR");

            var info = Editors.StartInfo(editor, @"C:\p", null, 0, 0, EditorEnvironment.None);

            Assert.That(EditorEnvironment.None.IsEmpty, Is.True);
            Assert.That(info.Environment["PATH"], Is.EqualTo(inheritedPath), "the Skyboxes tab adds nothing to PATH");
            Assert.That(info.Environment.TryGetValue("AUDIOSURF_DIR", out var game) ? game : null, Is.EqualTo(inheritedGame));
            Assert.That(ScriptDevLike(new[] { @"C:\bin" }, null).IsEmpty, Is.False);
            Assert.That(ScriptDevLike(new string[0], null).IsEmpty, Is.True, "no folders and a null variable add nothing");
        }

        [TestCase("vscode", new[] { @"C:\p", "-g", @"C:\p\src\draw\rail.lua:12:5" })]
        [TestCase("cursor", new[] { @"C:\p", "-g", @"C:\p\src\draw\rail.lua:12:5" })]
        [TestCase("sublime", new[] { @"C:\p", @"C:\p\src\draw\rail.lua:12:5" })]
        [TestCase("notepadpp", new[] { "-n12", "-c5", @"C:\p\src\draw\rail.lua" })]
        [TestCase("neovim", new[] { "+call cursor(12,5)", @"C:\p\src\draw\rail.lua" })]
        public void OpensAFileOfTheProjectAtItsLine(string id, string[] expected)
        {
            TouchAll();
            var editor = Editors.Detect(_search).Single(e => e.Id == id);

            var info = Editors.StartInfo(editor, @"C:\p", "src/draw/rail.lua", 12, 5, ScriptDevLike(new[] { @"C:\bin" }, null));

            Assert.That(info.ArgumentList, Is.EqualTo(expected));
            Assert.That(info.WorkingDirectory, Is.EqualTo(@"C:\p"));
        }

        [Test]
        public void AnUnknownPositionOpensAtTheTop()
        {
            TouchAll();
            var editor = Editors.Detect(_search).Single(e => e.Id == "vscode");

            var info = Editors.StartInfo(editor, @"C:\p", "twproject.json", 0, 0, ScriptDevLike(new[] { @"C:\bin" }, null));

            Assert.That(info.ArgumentList.Last(), Is.EqualTo(@"C:\p\twproject.json:1:1"));
        }

        [TestCase("vscode", true)]
        [TestCase("cursor", true)]
        [TestCase("sublime", false)]
        [TestCase("notepadpp", false)]
        [TestCase("neovim", false)]
        public void OnlyASingleProcessEditorWithATerminalCanMissTheEnvironment(string id, bool warns)
        {
            TouchAll();
            Assert.That(Editors.Detect(_search).Single(e => e.Id == id).KeepsOneProcessWithTerminal, Is.EqualTo(warns));
        }

        /// <summary>What the Script Dev tab hands an editor: its tool folders first on PATH, and the game folder.</summary>
        private static EditorEnvironment ScriptDevLike(string[] toolFolders, string gameFolder)
        {
            return new EditorEnvironment
            {
                PathFront = toolFolders,
                Variables = new Dictionary<string, string> { ["AUDIOSURF_DIR"] = gameFolder }
            };
        }

        private void TouchAll()
        {
            Touch("Local", "Programs", "Microsoft VS Code", "Code.exe");
            Touch("Local", "Programs", "cursor", "Cursor.exe");
            Touch("Program Files", "Sublime Text", "sublime_text.exe");
            Touch("Program Files", "Notepad++", "notepad++.exe");
            Touch("Program Files", "Neovim", "bin", "nvim.exe");
        }
    }
}
