using NUnit.Framework;
using TweakerCore.Skyboxes;

namespace TweakerCore.Tests.Skyboxes
{
    [TestFixture]
    public class SkyWireTests
    {
        [Test]
        public void CommandsMatchThePluginGrammar()
        {
            Assert.That(SkyWire.Enable(true), Is.EqualTo("SKY_ENABLE true"));
            Assert.That(SkyWire.Enable(false), Is.EqualTo("SKY_ENABLE false"));
            Assert.That(SkyWire.Rescan(), Is.EqualTo("SKY_RESCAN"));
            Assert.That(SkyWire.Select(new SkySelection(SkyKind.Package, "My Sky.sky")), Is.EqualTo("SKY_SELECT package My%20Sky.sky"));
            Assert.That(SkyWire.Select(new SkySelection(SkyKind.Packed, "skyboxes/cloudy_01.png")), Is.EqualTo("SKY_SELECT packed skyboxes%2Fcloudy_01.png"));
            Assert.That(SkyWire.Select(new SkySelection(SkyKind.ShaderFile, "небо.hlsl")), Is.EqualTo("SKY_SELECT shader_file %D0%BD%D0%B5%D0%B1%D0%BE.hlsl"));
        }

        [TestCase("program", SkyKind.Program)]
        [TestCase("packed", SkyKind.Packed)]
        [TestCase("shader_file", SkyKind.ShaderFile)]
        [TestCase("file", SkyKind.File)]
        [TestCase("face_dir", SkyKind.FaceDir)]
        [TestCase("package", SkyKind.Package)]
        public void CatalogReadsEveryKind(string key, SkyKind kind)
        {
            Assert.That(SkyWire.TryParseEntry($"SKY_ENTRY {key} My%20Sky.sky My%20Sky 256", out var entry), Is.True);
            Assert.That(entry, Is.EqualTo(new SkyCatalogEntry(kind, "My Sky.sky", "My Sky", 256)));
        }

        [TestCase("off", SkyLiveStatus.Off)]
        [TestCase("waiting", SkyLiveStatus.Waiting)]
        [TestCase("compiling", SkyLiveStatus.Compiling)]
        [TestCase("ready", SkyLiveStatus.Ready)]
        [TestCase("failed", SkyLiveStatus.Failed)]
        [TestCase("missing", SkyLiveStatus.Missing)]
        public void StateReadsEveryStatus(string key, SkyLiveStatus status)
        {
            Assert.That(SkyWire.TryParseState($"SKY_STATE true package My%20Sky.sky {key} sky%3A2%20%D0%B6", out var state), Is.True);
            Assert.That(state, Is.EqualTo(new SkyLiveState(true, new SkySelection(SkyKind.Package, "My Sky.sky"), status, "sky:2 ж")));
        }

        [Test]
        public void OptionalFieldsAndEmptyCatalog()
        {
            Assert.That(SkyWire.TryParseState("SKY_STATE false program gradient off", out var state), Is.True);
            Assert.That(state.Enabled, Is.False);
            Assert.That(state.Message, Is.Empty);
            Assert.That(SkyWire.TryParseEntry("SKY_ENTRY program gradient Gradient", out var entry), Is.True);
            Assert.That(entry.FaceSize, Is.Zero);
            Assert.That(SkyWire.TryParseSynced("SKY_SYNCED 0", out var count), Is.True);
            Assert.That(count, Is.Zero);
        }

        [TestCase(null)]
        [TestCase("")]
        [TestCase("SKY_STATE true package x")]
        [TestCase("SKY_STATE 1 package x ready")]
        [TestCase("SKY_STATE true unknown x ready")]
        [TestCase("SKY_STATE true package x future")]
        [TestCase("SKY_STATE true package x ready a b")]
        [TestCase("SKY_STATE true package x%00 ready")]
        [TestCase("SKY_STATE true package x failed x%00")]
        public void RejectsMalformedState(string line) => Assert.That(SkyWire.TryParseState(line, out _), Is.False);

        [TestCase("SKY_ENTRY package x")]
        [TestCase("SKY_ENTRY unknown x X")]
        [TestCase("SKY_ENTRY file x X -1")]
        [TestCase("SKY_ENTRY file x X 2147483648")]
        [TestCase("SKY_ENTRY file x X 1 extra")]
        [TestCase("SKY_ENTRY package x%00 X")]
        public void RejectsMalformedEntry(string line) => Assert.That(SkyWire.TryParseEntry(line, out _), Is.False);

        [TestCase("SKY_SYNCED -1")]
        [TestCase("SKY_SYNCED 1 extra")]
        [TestCase("SKY_SYNCED 2147483648")]
        [TestCase("SKY_SYNCED")]
        public void RejectsMalformedSynced(string line) => Assert.That(SkyWire.TryParseSynced(line, out _), Is.False);
    }
}
