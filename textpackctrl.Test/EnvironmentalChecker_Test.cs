namespace textpackctrl.Test
{
    using FolderChecker;
    using NUnit.Framework;
    using System.IO;
    using System;
    using System.Threading;
    using System.Threading.Tasks;

    [TestFixture]
    public class EnvironmentalChecker_Test
    {
        public static Random r = new Random();

        [Test]
        [Repeat(50)]
        public void EnvironmentalCheckerDetectChanges()
        {
            var pathToFolder = @"C:\Users\IAfon\OneDrive\Документы\Test folder";
            var control = FolderHashInfo.Create(pathToFolder);
            File.WriteAllText(pathToFolder + @"\Test1.txt", r.NextDouble().ToString());
            var afterChange = FolderHashInfo.Create(pathToFolder);
            Assert.IsFalse(control.Equals(afterChange));
        }

        [Test]
        [Repeat(10)]
        public void FolderHashInfoCanDetectSerializedFile()
        {
            var pathToFolder = @"C:\Users\IAfon\OneDrive\Документы\Test folder";
            var state = FolderHashInfo.Create(pathToFolder);
            state.Save(pathToFolder);
            Assert.IsTrue(FolderHashInfo.TryFind(pathToFolder, out FolderHashInfo result));
        }

        [Test]
        [Repeat(10)]
        public void EnvironmentalCheckerCanDetectChangerViaHINFFile()
        {
            var pathToFolder = @"C:\Users\IAfon\OneDrive\Документы\Test folder";
            if (FolderHashInfo.TryFind(pathToFolder, out FolderHashInfo control))
            {
                File.WriteAllText(pathToFolder + @"\Test1.txt", r.NextDouble().ToString());
                var changedState = FolderHashInfo.Create(pathToFolder);
                Assert.IsFalse(control.Equals(changedState));
            }
            else
            {
                Assert.Fail("FolderHashInfo can not detect existing HINF file");
            }
        }
    }
}
