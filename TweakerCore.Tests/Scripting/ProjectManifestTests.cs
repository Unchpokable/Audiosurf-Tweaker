namespace TweakerCore.Tests.Scripting
{
    using System.Collections.Generic;
    using System.Text;
    using System.Text.Json;
    using NUnit.Framework;
    using TweakerCore.Scripting;

    /// <summary>twproject.json edited field by field: the values change, nothing else in the file does.</summary>
    [TestFixture]
    public class ProjectManifestTests
    {
        private static string Edit(string text, params (string Field, string Value)[] changes)
        {
            var map = new Dictionary<string, string>();
            foreach (var (field, value) in changes)
                map[field] = value;
            return Encoding.UTF8.GetString(ProjectManifest.Edit(Encoding.UTF8.GetBytes(text), map));
        }

        private const string Manifest = """
            {
              "$schema": ".twllink/twproject.schema.json",
              // my own note, kept
              "format": 1,
              "id": "my_hud",
              "name": "my_hud",
              "author": "me",
              "version": "0.1.0",
              "description": "What it does, on one line.",
              "api": 1,
              "build": { "comments": "strip", "embed": ["LICENSE"] }
            }
            """;

        [Test]
        public void ChangesOnlyTheValuesAskedFor()
        {
            var edited = Edit(Manifest, ("name", "My HUD"), ("version", "1.2.0"), ("description", "Draws the score."));

            var expected = Manifest
                .Replace("\"name\": \"my_hud\"", "\"name\": \"My HUD\"")
                .Replace("\"0.1.0\"", "\"1.2.0\"")
                .Replace("What it does, on one line.", "Draws the score.");
            Assert.That(edited, Is.EqualTo(expected));
        }

        [Test]
        public void TextThatNeedsEscapingIsEscapedAndCyrillicStaysReadable()
        {
            var edited = Edit(Manifest, ("name", "Мой \"HUD\" \\ v2"));

            Assert.That(edited, Does.Contain("\"name\": \"Мой \\\"HUD\\\" \\\\ v2\""));
            using var document = JsonDocument.Parse(edited, new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip });
            Assert.That(document.RootElement.GetProperty("name").GetString(), Is.EqualTo("Мой \"HUD\" \\ v2"));
        }

        [Test]
        public void AnAlreadyEscapedValueIsReplacedWhole()
        {
            var edited = Edit("{ \"name\": \"a \\\"b\\\" \\u0063\", \"api\": 1 }", ("name", "plain"));

            Assert.That(edited, Is.EqualTo("{ \"name\": \"plain\", \"api\": 1 }"));
        }

        [Test]
        public void AMissingFieldIsAddedAfterTheLastOneWithTheSameIndent()
        {
            var edited = Edit("{\n    \"id\": \"x\",\n    \"api\": 1\n}\n", ("author", "me"), ("description", "Does it."));

            Assert.That(edited, Is.EqualTo("{\n    \"id\": \"x\",\n    \"api\": 1,\n    \"author\": \"me\",\n    \"description\": \"Does it.\"\n}\n"));
        }

        [Test]
        public void ANestedFieldOfTheSameNameIsNotTouched()
        {
            var edited = Edit("{ \"build\": { \"name\": \"inner\" }, \"name\": \"outer\" }", ("name", "new"));

            Assert.That(edited, Is.EqualTo("{ \"build\": { \"name\": \"inner\" }, \"name\": \"new\" }"));
        }

        [Test]
        public void AByteOrderMarkSurvives()
        {
            var bytes = Encoding.UTF8.GetPreamble();
            var text = Encoding.UTF8.GetBytes("{ \"name\": \"a\" }");
            var input = new byte[bytes.Length + text.Length];
            bytes.CopyTo(input, 0);
            text.CopyTo(input, bytes.Length);

            var edited = ProjectManifest.Edit(input, new Dictionary<string, string> { ["name"] = "b" });

            Assert.That(edited[0], Is.EqualTo(0xEF));
            Assert.That(Encoding.UTF8.GetString(edited, 3, edited.Length - 3), Is.EqualTo("{ \"name\": \"b\" }"));
        }

        [TestCase("id", "my_hud", true)]
        [TestCase("id", "My_HUD", false)]
        [TestCase("id", "my hud", false)]
        [TestCase("id", "", false)]
        [TestCase("version", "1.0.0", true)]
        [TestCase("version", "1.0", false)]
        [TestCase("version", "01.0.0", false)]
        [TestCase("name", "PuzzlePro HUD", true)]
        [TestCase("name", "  ", false)]
        [TestCase("description", "two\nlines", false)]
        public void TheRulesAreTwllinks(string field, string value, bool ok)
        {
            Assert.That(ProjectManifest.Problem(field, value) == null, Is.EqualTo(ok));
        }

        [TestCase("My HUD!", "my_hud")]
        [TestCase("  PuzzlePro   HUD  ", "puzzlepro_hud")]
        [TestCase("Счётчик", "")]
        public void AnIdIsDerivedFromANameAsTwllinkDerivesItFromAFolder(string name, string id)
        {
            Assert.That(ProjectManifest.IdFrom(name), Is.EqualTo(id));
        }
    }
}
