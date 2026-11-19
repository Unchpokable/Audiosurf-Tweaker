using System;
using System.IO;
using System.Linq;
using System.Xml;
using NUnit.Framework;
using TweakerCore.Configuration;

namespace TweakerCore.Tests
{
    public class ConfigurationTests
    {
        [Test]
        public void XmlSettingsPreserveUnknownSectionsAndReopen()
        {
            var directory = Path.Combine(Path.GetTempPath(), "tweaker-settings-" + Guid.NewGuid());
            Directory.CreateDirectory(directory);
            var path = Path.Combine(directory, "TweakerUI.exe.config");
            try
            {
                File.WriteAllText(path, "<configuration><!--user comment--><custom value='keep'/><appSettings>"
                    + "<add key='DarkTheme' value='true'/><add key='UserKey' value='&amp;'/><add key='UWPNotificationsAllowed' value='true'/>"
                    + "</appSettings></configuration>");
                var store = new XmlSettingsStore(path);
                Assert.That(store.Ensure("DarkTheme", "false"), Is.False);
                Assert.That(store.Ensure("NewSetting", "default"), Is.True);
                store.Set("TexturesPath", @"C:\Игры & музыка\engine\textures");
                store.Remove("UWPNotificationsAllowed");
                store.Save();

                var reopened = new XmlSettingsStore(path);
                Assert.Multiple(() =>
                {
                    Assert.That(reopened.Get("DarkTheme"), Is.EqualTo("true"));
                    Assert.That(reopened.Get("UserKey"), Is.EqualTo("&"));
                    Assert.That(reopened.Get("NewSetting"), Is.EqualTo("default"));
                    Assert.That(reopened.Get("TexturesPath"), Is.EqualTo(@"C:\Игры & музыка\engine\textures"));
                    Assert.That(reopened.Get("UWPNotificationsAllowed"), Is.Null);
                    Assert.That(File.ReadAllText(path), Does.Contain("<!--user comment-->"));
                    Assert.That(File.ReadAllText(path), Does.Contain("<custom value="));
                    Assert.That(Directory.GetFiles(directory), Has.Length.EqualTo(1));
                });

                File.WriteAllText(path, "<configuration><appSettings>");
                Assert.Throws<XmlException>(() => new XmlSettingsStore(path));
                Assert.That(File.ReadAllText(path), Is.EqualTo("<configuration><appSettings>"));
            }
            finally
            {
                Directory.Delete(directory, true);
            }
        }

        [Test]
        public void MissingXmlIsCreatedWithDefaults()
        {
            var path = Path.Combine(Path.GetTempPath(), "tweaker-" + Guid.NewGuid() + ".config");
            try
            {
                var store = new XmlSettingsStore(path);
                store.Ensure("FirstRun", "true");
                store.Save();
                Assert.That(new XmlSettingsStore(path).Get("FirstRun"), Is.EqualTo("true"));
            }
            finally
            {
                File.Delete(path);
            }
        }

        [Test]
        public void FailedXmlReplacementLeavesOriginalAndNoStagingFile()
        {
            var directory = Path.Combine(Path.GetTempPath(), "tweaker-readonly-" + Guid.NewGuid());
            Directory.CreateDirectory(directory);
            var path = Path.Combine(directory, "settings.config");
            const string original = "<configuration><appSettings><add key='value' value='original'/></appSettings></configuration>";
            try
            {
                File.WriteAllText(path, original);
                var store = new XmlSettingsStore(path);
                store.Set("value", "changed");
                File.SetAttributes(path, FileAttributes.ReadOnly);

                Assert.Throws<UnauthorizedAccessException>(() => store.Save());
                Assert.That(File.ReadAllText(path), Is.EqualTo(original));
                Assert.That(Directory.GetFiles(directory), Has.Length.EqualTo(1));
            }
            finally
            {
                File.SetAttributes(path, FileAttributes.Normal);
                Directory.Delete(directory, true);
            }
        }

        [Test]
        public void OldSteamLayoutChecksCandidateDirectories()
        {
            var directory = Path.Combine(Path.GetTempPath(), "tweaker-steam-" + Guid.NewGuid());
            Directory.CreateDirectory(Path.Combine(directory, "steamapps", "common", "Audiosurf"));
            try
            {
                var escaped = directory.Replace("\\", "\\\\");
                var text = "LibraryFolders { TimeNextStatsReport 0 0 \"missing\" 1 \"" + escaped + "\" }";
                Assert.That(SteamLibraryFolders.AudiosurfLibraries(text).ToArray(), Is.EqualTo(new[] { directory }));
            }
            finally
            {
                Directory.Delete(directory, true);
            }
        }

        [Test]
        public void SteamLibrariesHandleUnknownFieldsDuplicatesAndEscapedPaths()
        {
            var text = "\uFEFF// Steam libraries\n\"libraryfolders\" {"
                + " 0 { path \"C:\\\\Steam\" apps { 100 \"1\" } }"
                + " 1 { path \"D:\\\\Игры\" label \"a \\\"quote\\\"\" unknown { nested { x y } } apps { 12900 \"42\" } }"
                + " 1 { path \"E:\\Games\" apps { 12900 \"43\" } }"
                + " 2 { path \"F:\\\\Steam\" } }";
            Assert.That(SteamLibraryFolders.AudiosurfLibraries(text).ToArray(), Is.EqualTo(new[] { @"D:\Игры", @"E:\Games" }));
            Assert.That(SteamLibraryFolders.AudiosurfLibraries(""), Is.Empty);
        }

        [TestCase("libraryfolders { 0 { path \"unfinished")]
        [TestCase("libraryfolders { 0 { path value }")]
        [TestCase("libraryfolders { } }")]
        [TestCase("#include other.vdf")]
        [TestCase("libraryfolders { 0 path [$WIN32] }")]
        public void SteamLibrariesRejectMalformedOrUnsupportedInput(string text)
        {
            var exception = Assert.Throws<FormatException>(() => SteamLibraryFolders.AudiosurfLibraries(text).ToArray());
            Assert.That(exception.Message, Does.Contain("character"));
        }
    }
}
