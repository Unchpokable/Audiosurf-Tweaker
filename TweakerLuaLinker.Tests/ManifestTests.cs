using System.Linq;

using NUnit.Framework;

using TweakerLuaLinker.Diagnostics;
using TweakerLuaLinker.Project;

using static TweakerLuaLinker.Tests.Assertions;

namespace TweakerLuaLinker.Tests
{
    [TestFixture]
    public class ManifestTests
    {
        private const string Hello = "local x = 1\n";

        [Test]
        public void MinimalManifestBuilds()
        {
            using var project = new TestProject().Main(Hello);
            var result = project.Link();

            Succeeded(result);
            Assert.That(result.Diagnostics, Is.Empty);
            var m = result.Manifest!;
            Assert.That((m.Id, m.Name, m.Author, m.Version, m.Description, m.Api), Is.EqualTo(("demo", "Demo HUD", "Tester", "1.2.3", "A demo", 1)));
            Assert.That(m.Comments, Is.EqualTo(CommentMode.Keep));
            Assert.That(m.OutputFileName, Is.EqualTo("demo.lua"));
        }

        [Test]
        public void MissingManifestIsTW0001()
        {
            using var project = new TestProject().Main(Hello);
            System.IO.File.Delete(System.IO.Path.Combine(project.Root, "twproject.json"));
            Single(project.Link(), DiagnosticCodes.ManifestMissing);
        }

        [Test]
        public void CommentsAndTrailingCommasAreAllowed()
        {
            using var project = new TestProject().Main(Hello).RawManifest("""
                {
                  // a comment
                  "format": 1, "id": "demo", "name": "N", "author": "A", "version": "1.0.0",
                  "description": "D", "api": 1, /* another */
                }
                """);
            Succeeded(project.Link());
        }

        [Test]
        public void BrokenJsonIsTW0002WithPosition()
        {
            using var project = new TestProject().Main(Hello).RawManifest("{\n  \"format\": 1,\n  \"id\" \"demo\"\n}\n");
            var d = Single(project.Link(), DiagnosticCodes.ManifestSyntax);
            Assert.That(d.Line, Is.EqualTo(3));
            Assert.That(d.Column, Is.GreaterThan(0));
        }

        [Test]
        public void NonObjectIsTW0002()
        {
            using var project = new TestProject().Main(Hello).RawManifest("[1, 2]");
            Single(project.Link(), DiagnosticCodes.ManifestSyntax);
        }

        [Test]
        public void UnsupportedFormatIsTW0003()
        {
            using var project = new TestProject().Main(Hello).Field("format", "2");
            Single(project.Link(), DiagnosticCodes.ManifestFormat);
        }

        [TestCase("format")]
        [TestCase("id")]
        [TestCase("name")]
        [TestCase("author")]
        [TestCase("version")]
        [TestCase("description")]
        [TestCase("api")]
        public void EveryRequiredFieldIsRequired(string field)
        {
            using var project = new TestProject().Main(Hello).Field(field, null);
            var d = Single(project.Link(), DiagnosticCodes.ManifestFieldMissing);
            Assert.That(d.Message, Does.Contain($"'{field}'"));
        }

        [TestCase("id", "5")]
        [TestCase("name", "true")]
        [TestCase("api", "\"1\"")]
        [TestCase("api", "1.5")]
        [TestCase("build", "[]")]
        public void WrongTypeIsTW0005(string field, string raw)
        {
            using var project = new TestProject().Main(Hello).Field(field, raw);
            Single(project.Link(), DiagnosticCodes.ManifestFieldType);
        }

        [TestCase("")]
        [TestCase("Demo")]
        [TestCase("my-hud")]
        [TestCase("my hud")]
        [TestCase("../evil")]
        public void BadIdIsTW0006(string id)
        {
            using var project = new TestProject().Main(Hello).Field("id", $"\"{id}\"");
            Single(project.Link(), DiagnosticCodes.ManifestId);
        }

        [TestCase("1.7")]
        [TestCase("1")]
        [TestCase("1.2.3.4")]
        [TestCase("1.2.3-beta")]
        [TestCase("v1.2.3")]
        [TestCase("01.2.3")]
        [TestCase("1.02.3")]
        [TestCase("1..3")]
        [TestCase(" 1.2.3")]
        public void NonSemverVersionIsTW0007(string version)
        {
            using var project = new TestProject().Main(Hello).Field("version", $"\"{version}\"");
            Single(project.Link(), DiagnosticCodes.ManifestVersion);
        }

        [TestCase("0.0.0")]
        [TestCase("1.7.0")]
        [TestCase("10.20.30")]
        public void SemverVersionIsAccepted(string version)
        {
            using var project = new TestProject().Main(Hello).Field("version", $"\"{version}\"");
            Succeeded(project.Link());
        }

        [TestCase("name", "\"two\\nlines\"")]
        [TestCase("author", "\"   \"")]
        [TestCase("description", "\"tab\\there\"")]
        public void HeaderTextMustBeOneLineIsTW0008(string field, string raw)
        {
            using var project = new TestProject().Main(Hello).Field(field, raw);
            Single(project.Link(), DiagnosticCodes.ManifestText);
        }

        [TestCase("0")]
        [TestCase("-1")]
        public void NonPositiveApiIsTW0009(string api)
        {
            using var project = new TestProject().Main(Hello).Field("api", api);
            Single(project.Link(), DiagnosticCodes.ManifestApi);
        }

        [Test]
        public void UnknownFieldIsAWarningNotAFailure()
        {
            using var project = new TestProject().Main(Hello).Field("entry", "\"init\"").Field("$schema", "\"x.json\"");
            var result = project.Link();

            Succeeded(result);
            var d = Single(result, DiagnosticCodes.ManifestUnknownField);
            Assert.That(d.Severity, Is.EqualTo(Severity.Warning));
            Assert.That(d.Message, Does.Contain("'entry'"));
        }

        [Test]
        public void UnknownNestedFieldNamesItsSection()
        {
            using var project = new TestProject().Main(Hello).Field("build", "{\"coments\": \"strip\"}");
            Assert.That(Single(project.Link(), DiagnosticCodes.ManifestUnknownField).Message, Does.Contain("'build.coments'"));
        }

        [TestCase("\"skip\"")]
        [TestCase("\"Strip\"")]
        [TestCase("true")]
        public void BadCommentsValueIsTW0011(string raw)
        {
            using var project = new TestProject().Main(Hello).Field("build", $"{{\"comments\": {raw}}}");
            Single(project.Link(), DiagnosticCodes.ManifestComments);
        }

        [Test]
        public void EmbedOfMissingFileIsTW0012()
        {
            using var project = new TestProject().Main(Hello).Field("build", "{\"embed\": [\"LICENSE\"]}");
            Single(project.Link(), DiagnosticCodes.EmbedMissing);
        }

        [TestCase("../outside.txt")]
        [TestCase("C:/Windows/win.ini")]
        public void EmbedOutsideTheProjectIsTW0013(string path)
        {
            using var project = new TestProject().Main(Hello).Field("build", $"{{\"embed\": [\"{path}\"]}}");
            Single(project.Link(), DiagnosticCodes.EmbedOutsideProject);
        }

        [Test]
        public void EmbedOfBinaryIsTW0014()
        {
            using var project = new TestProject().Main(Hello)
                .WriteBytes("logo.png", new byte[] { 0x89, 0x50, 0x4E, 0x47, 0x00, 0x00 })
                .Field("build", "{\"embed\": [\"logo.png\"]}");
            Single(project.Link(), DiagnosticCodes.EmbedNotText);
        }

        [TestCase("TW9999")]
        [TestCase("TW0104")]
        public void SuppressingAnUnknownCodeOrAnErrorIsTW0015(string code)
        {
            using var project = new TestProject().Main(Hello).Field("diagnostics", $"{{\"suppress\": [\"{code}\"]}}");
            var result = project.Link();
            Succeeded(result);
            Single(result, DiagnosticCodes.SuppressInvalid);
        }

        [Test]
        public void SuppressedWarningDisappears()
        {
            using var project = new TestProject().Main(Hello).Write("src/orphan.lua", "")
                .Field("diagnostics", "{\"suppress\": [\"TW0109\"]}");
            var result = project.Link();
            Succeeded(result);
            Assert.That(result.Diagnostics, Is.Empty);
        }

        [Test]
        public void WarningsAsErrorsFailsTheBuild()
        {
            using var project = new TestProject().Main(Hello).Write("src/orphan.lua", "")
                .Field("diagnostics", "{\"warnings_as_errors\": true}");
            var result = project.Link();

            Assert.That(result.Success, Is.False);
            Assert.That(Single(result, DiagnosticCodes.UnreachableModule).Severity, Is.EqualTo(Severity.Error));
        }

        [Test]
        public void EveryProblemIsReportedAtOnce()
        {
            using var project = new TestProject().Main(Hello).Field("id", "\"Bad\"").Field("version", "\"1.0\"").Field("api", "0");
            var codes = project.Link().Diagnostics.Select(d => d.Code).ToList();
            Assert.That(codes, Is.EquivalentTo(new[] { DiagnosticCodes.ManifestId, DiagnosticCodes.ManifestVersion, DiagnosticCodes.ManifestApi }));
        }
    }
}
