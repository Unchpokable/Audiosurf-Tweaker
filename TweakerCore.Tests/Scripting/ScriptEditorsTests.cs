namespace TweakerCore.Tests.Scripting
{
    using System;
    using System.IO;
    using System.Linq;
    using NUnit.Framework;
    using TweakerCore.Scripting;

    /// <summary>
    /// Finding editors in fake install folders, and the command lines and environment they are started with.
    /// Nothing is launched: what an editor does with its arguments is the editor's, and the owner checks that live.
    /// </summary>
    [TestFixture]
    public class ScriptEditorsTests
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

            var found = ScriptEditors.Detect(_search);

            Assert.That(found.Select(e => e.Id), Is.EqualTo(new[] { "vscode", "notepadpp", "neovim" }));
            Assert.That(found[0].ExecutablePath, Does.EndWith(Path.Combine("Microsoft VS Code", "Code.exe")));
        }

        [Test]
        public void FindsAnEditorThroughItsShimOnPath()
        {
            var cursor = Touch("Elsewhere", "cursor", "Cursor.exe");
            Touch("Elsewhere", "cursor", "resources", "app", "bin", "cursor.cmd");
            _search = new EditorSearch { PathVariable = "C:\\nothing;" + Path.Combine(_root, "Elsewhere", "cursor", "resources", "app", "bin") };

            var editor = ScriptEditors.Detect(_search).Single();

            Assert.That(editor.Id, Is.EqualTo("cursor"));
            Assert.That(editor.ExecutablePath, Is.EqualTo(Path.GetFullPath(cursor)));
        }

        [Test]
        public void AShimLeadingNowhereFindsNothing()
        {
            Touch("Stray", "bin", "code.cmd");
            _search = new EditorSearch { PathVariable = Path.Combine(_root, "Stray", "bin") };

            Assert.That(ScriptEditors.Detect(_search), Is.Empty);
        }

        [Test]
        public void GitsOwnVimIsNotAnEditor()
        {
            Touch("Git", "git-bash.exe");
            Touch("Git", "usr", "bin", "vim.exe");
            var real = Touch("Tools", "vim", "vim.exe");
            _search = new EditorSearch { PathVariable = Path.Combine(_root, "Git", "usr", "bin") + ";" + Path.Combine(_root, "Tools", "vim") };

            Assert.That(ScriptEditors.Detect(_search).Single().ExecutablePath, Is.EqualTo(Path.GetFullPath(real)));
        }

        [Test]
        public void TheNewestVimIsTaken()
        {
            Touch("Program Files", "Vim", "vim90", "vim.exe");
            var newest = Touch("Program Files", "Vim", "vim91", "vim.exe");

            Assert.That(ScriptEditors.Detect(_search).Single().ExecutablePath, Is.EqualTo(Path.GetFullPath(newest)));
        }

        [Test]
        public void TheEditorGetsTheToolFoldersFirstOnPathAndTheGameFolder()
        {
            Touch("Local", "Programs", "Microsoft VS Code", "Code.exe");
            var editor = ScriptEditors.Detect(_search).Single();

            var tools = new[] { @"C:\Tweaker\DevTools\Binaries", @"C:\Tweaker\DevTools\CgrPy" };
            var info = ScriptEditors.StartInfo(editor, @"C:\p\my_hud", null, 0, 0, tools, @"D:\Games\Audiosurf");

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
            var editor = ScriptEditors.Detect(_search).Single();
            var inherited = Environment.GetEnvironmentVariable("AUDIOSURF_DIR");

            var info = ScriptEditors.StartInfo(editor, @"C:\p", null, 0, 0, new[] { @"C:\bin" }, null);

            Assert.That(info.Environment.TryGetValue("AUDIOSURF_DIR", out var value) ? value : null, Is.EqualTo(inherited));
        }

        [TestCase("vscode", new[] { @"C:\p", "-g", @"C:\p\src\draw\rail.lua:12:5" })]
        [TestCase("cursor", new[] { @"C:\p", "-g", @"C:\p\src\draw\rail.lua:12:5" })]
        [TestCase("sublime", new[] { @"C:\p", @"C:\p\src\draw\rail.lua:12:5" })]
        [TestCase("notepadpp", new[] { "-n12", "-c5", @"C:\p\src\draw\rail.lua" })]
        [TestCase("neovim", new[] { "+call cursor(12,5)", @"C:\p\src\draw\rail.lua" })]
        public void OpensAFileOfTheProjectAtItsLine(string id, string[] expected)
        {
            TouchAll();
            var editor = ScriptEditors.Detect(_search).Single(e => e.Id == id);

            var info = ScriptEditors.StartInfo(editor, @"C:\p", "src/draw/rail.lua", 12, 5, new[] { @"C:\bin" }, null);

            Assert.That(info.ArgumentList, Is.EqualTo(expected));
            Assert.That(info.WorkingDirectory, Is.EqualTo(@"C:\p"));
        }

        [Test]
        public void AnUnknownPositionOpensAtTheTop()
        {
            TouchAll();
            var editor = ScriptEditors.Detect(_search).Single(e => e.Id == "vscode");

            var info = ScriptEditors.StartInfo(editor, @"C:\p", "twproject.json", 0, 0, new[] { @"C:\bin" }, null);

            Assert.That(info.ArgumentList.Last(), Is.EqualTo(@"C:\p\twproject.json:1:1"));
        }

        [TestCase("vscode", true)]
        [TestCase("cursor", true)]
        [TestCase("sublime", false)]
        [TestCase("notepadpp", false)]
        [TestCase("neovim", false)]
        public void OnlyASingleProcessEditorWithATerminalWarnsWhenRunning(string id, bool warns)
        {
            TouchAll();
            Assert.That(ScriptEditors.Detect(_search).Single(e => e.Id == id).TerminalMissesPathWhenRunning, Is.EqualTo(warns));
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
