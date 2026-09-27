namespace TweakerCore.Tests
{
    using TweakerCore.FolderChecker;
    using NUnit.Framework;
    using System.IO;
    using System;

    [TestFixture]
    public class EnvironmentalChecker_Test
    {
        public static Random r = new Random();

        private static readonly string TestFolderPath = Path.Combine(Path.GetTempPath(), "AudiosurfTweakerTests", "FolderChecker");

        [SetUp]
        public void SetUp()
        {
            Directory.CreateDirectory(TestFolderPath);
        }

        [TearDown]
        public void TearDown()
        {
            if (Directory.Exists(TestFolderPath))
                Directory.Delete(TestFolderPath, recursive: true);
        }

        [Test]
        [Repeat(50)]
        public void EnvironmentalCheckerDetectChanges()
        {
            var control = FolderHashInfo.Create(TestFolderPath);
            File.WriteAllText(Path.Combine(TestFolderPath, "Test1.txt"), r.NextDouble().ToString());
            var afterChange = FolderHashInfo.Create(TestFolderPath);
            Assert.IsFalse(control.Equals(afterChange));
        }

        [Test]
        [Repeat(10)]
        public void FolderHashInfoCanDetectSerializedFile()
        {
            var state = FolderHashInfo.Create(TestFolderPath);
            state.Save(TestFolderPath);
            Assert.IsTrue(FolderHashInfo.TryFind(TestFolderPath, out FolderHashInfo result));
        }

        [Test]
        public void TryRenameState_MatchingName_RenamesAndKeepsHashes()
        {
            File.WriteAllText(Path.Combine(TestFolderPath, "tiles.png"), "tiles");
            FolderHashInfo.Create(TestFolderPath, "Old").Save(TestFolderPath);

            Assert.IsTrue(FolderHashInfo.TryRenameState(TestFolderPath, "Old", "New"));

            Assert.IsTrue(EnvironmentChecker.CheckEnvironment(TestFolderPath, out FolderHashInfo state));
            Assert.AreEqual("New", state.StateName);
        }

        [Test]
        public void TryRenameState_DriftedFolder_StaysDrifted()
        {
            File.WriteAllText(Path.Combine(TestFolderPath, "tiles.png"), "tiles");
            FolderHashInfo.Create(TestFolderPath, "Old").Save(TestFolderPath);
            File.WriteAllText(Path.Combine(TestFolderPath, "tiles.png"), "edited by hand");

            Assert.IsTrue(FolderHashInfo.TryRenameState(TestFolderPath, "Old", "New"));

            Assert.IsFalse(EnvironmentChecker.CheckEnvironment(TestFolderPath, out FolderHashInfo state));
            Assert.AreEqual("New", state.StateName);
        }

        [Test]
        public void TryRenameState_OtherName_LeavesStateAlone()
        {
            FolderHashInfo.Create(TestFolderPath, "Installed").Save(TestFolderPath);

            Assert.IsFalse(FolderHashInfo.TryRenameState(TestFolderPath, "Some other skin", "New"));

            Assert.IsTrue(FolderHashInfo.TryFind(TestFolderPath, out FolderHashInfo state));
            Assert.AreEqual("Installed", state.StateName);
        }

        [Test]
        public void TryRenameState_NoState_ReturnsFalse()
        {
            Assert.IsFalse(FolderHashInfo.TryRenameState(TestFolderPath, "Old", "New"));
        }

        [Test]
        [Repeat(10)]
        public void EnvironmentalCheckerCanDetectChangerViaHINFFile()
        {
            var control = FolderHashInfo.Create(TestFolderPath);
            control.Save(TestFolderPath);

            Assert.IsTrue(FolderHashInfo.TryFind(TestFolderPath, out FolderHashInfo found));

            File.WriteAllText(Path.Combine(TestFolderPath, "Test1.txt"), r.NextDouble().ToString());
            var changedState = FolderHashInfo.Create(TestFolderPath);
            Assert.IsFalse(found.Equals(changedState));
        }
    }
}
