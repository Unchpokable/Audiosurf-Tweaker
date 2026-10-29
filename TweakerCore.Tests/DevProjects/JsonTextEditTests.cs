namespace TweakerCore.Tests.DevProjects
{
    using System.Collections.Generic;
    using System.IO;
    using System.Text;
    using NUnit.Framework;
    using TweakerCore.DevProjects;

    /// <summary>
    /// The values that are not strings - a sky's "version", module.json's "enabled" and "sky" - edited in files with
    /// comments. Strings are covered through ProjectManifestTests, which go through the same edit.
    /// </summary>
    [TestFixture]
    public class JsonTextEditTests
    {
        private static string Edit(string text, params (string Key, string Value)[] changes)
        {
            var list = new List<KeyValuePair<string, string>>();
            foreach (var change in changes)
                list.Add(new KeyValuePair<string, string>(change.Key, change.Value));
            return Encoding.UTF8.GetString(JsonTextEdit.Edit(Encoding.UTF8.GetBytes(text), list, "test.json"));
        }

        [Test]
        public void ReplacesANumberABoolAndAnObjectAndLeavesTheCommentsAround()
        {
            const string before = "{\n  // the plugin's own\n  \"enabled\": true, /* on */\n  \"sky\": { \"kind\": \"packed\", \"id\": \"a\" },\n  \"version\": 3,\n}\n";

            var after = Edit(before,
                ("enabled", JsonTextEdit.Bool(false)),
                ("sky", "{ \"kind\": \"package\", \"id\": \"My Sky.sky\" }"),
                ("version", JsonTextEdit.Number(4)));

            Assert.That(after, Is.EqualTo(
                "{\n  // the plugin's own\n  \"enabled\": false, /* on */\n  \"sky\": { \"kind\": \"package\", \"id\": \"My Sky.sky\" },\n  \"version\": 4,\n}\n"));
        }

        [Test]
        public void AnAbsentFieldIsAddedAfterTheLastWithItsValueAsGiven()
        {
            var after = Edit("{\n    \"format\": 1\n}", ("version", JsonTextEdit.Number(1)), ("enabled", JsonTextEdit.Bool(true)));

            Assert.That(after, Is.EqualTo("{\n    \"format\": 1,\n    \"version\": 1,\n    \"enabled\": true\n}"));
        }

        [Test]
        public void AStringIsTrimmedAndKeepsCyrillicReadable()
        {
            Assert.That(JsonTextEdit.String("  Вася \"V\" "), Is.EqualTo("\"Вася \\\"V\\\"\""));
        }

        [Test]
        public void SomethingThatIsNotAnObjectIsInvalidData()
        {
            Assert.Throws<InvalidDataException>(() => Edit("[1, 2]", ("a", "1")));
            Assert.Throws<InvalidDataException>(() => Edit("not json", ("a", "1")));
        }
    }
}
