namespace TweakerCore.Tests.Scripting
{
    using NUnit.Framework;
    using TweakerCore.Scripting;

    /// <summary>
    /// ScriptWire against the plugin's side of the same grammar (TweakerPlugin/src/lua/lua_remote.cxx). The lines
    /// parsed here are the ones its offline harness (TweakerPlugin/harness/remote/remotetest.cxx) checks the plugin
    /// sends, byte for byte - so the two halves are pinned to one text.
    /// </summary>
    [TestFixture]
    public class ScriptWireTests
    {
        [Test]
        public void CommandsAreSpelledAsThePluginParsesThem()
        {
            Assert.That(ScriptWire.Set("puzzlepro_hud.lua", true), Is.EqualTo("SCRIPT_SET puzzlepro_hud.lua true"));
            Assert.That(ScriptWire.Set("puzzlepro_hud.lua", false), Is.EqualTo("SCRIPT_SET puzzlepro_hud.lua false"));
            Assert.That(ScriptWire.Reload("b.lua"), Is.EqualTo("SCRIPT_RELOAD b.lua"));
            Assert.That(ScriptWire.Rescan(), Is.EqualTo("SCRIPT_RESCAN"));
        }

        [Test]
        public void FileNamesArePercentEncoded()
        {
            // The plugin's harness sends "SCRIPT_SET my%20script.lua false" and finds "my script.lua".
            Assert.That(ScriptWire.Set("my script.lua", false), Is.EqualTo("SCRIPT_SET my%20script.lua false"));
            Assert.That(ScriptWire.Reload("скрипт.lua"), Is.EqualTo("SCRIPT_RELOAD %D1%81%D0%BA%D1%80%D0%B8%D0%BF%D1%82.lua"));
        }

        [Test]
        public void ReadsEveryStatus()
        {
            Assert.That(ScriptWire.TryParseState("SCRIPT_STATE a.lua true running", out var running), Is.True);
            Assert.That(running, Is.EqualTo(new ScriptLiveState("a.lua", true, ScriptLiveStatus.Running, "")));

            Assert.That(ScriptWire.TryParseState("SCRIPT_STATE c.lua false off", out var off), Is.True);
            Assert.That(off.Status, Is.EqualTo(ScriptLiveStatus.Off));
            Assert.That(off.Enabled, Is.False);

            Assert.That(ScriptWire.TryParseState("SCRIPT_STATE zz.lua false missing", out var missing), Is.True);
            Assert.That(missing.Status, Is.EqualTo(ScriptLiveStatus.Missing));
        }

        [Test]
        public void MessagesAreDecoded()
        {
            Assert.That(ScriptWire.TryParseState("SCRIPT_STATE b.lua true failed b.lua%3A2%3A%20boom", out var failed), Is.True);
            Assert.That(failed, Is.EqualTo(new ScriptLiveState("b.lua", true, ScriptLiveStatus.Failed, "b.lua:2: boom")));

            Assert.That(ScriptWire.TryParseState(
                "SCRIPT_STATE a.lua true suspended 5%20errors%20in%2012%20frames%2C%20the%20last%3A%20a.lua%3A7%3A%20oops", out var suspended), Is.True);
            Assert.That(suspended.Status, Is.EqualTo(ScriptLiveStatus.Suspended));
            Assert.That(suspended.Message, Is.EqualTo("5 errors in 12 frames, the last: a.lua:7: oops"));

            Assert.That(ScriptWire.TryParseState("SCRIPT_STATE my%20script.lua false off", out var spaced), Is.True);
            Assert.That(spaced.FileName, Is.EqualTo("my script.lua"));

            Assert.That(ScriptWire.TryParseState("SCRIPT_STATE x.lua false failed x%D0%B6%D0%B6", out var utf8), Is.True);
            Assert.That(utf8.Message, Is.EqualTo("xжж"));
        }

        [Test]
        public void AFailedEnableComesWithFalse()
        {
            // overlay-scripts.md: an enable that did not take springs back - "false failed <error>".
            Assert.That(ScriptWire.TryParseState("SCRIPT_STATE b.lua false failed oops", out var state), Is.True);
            Assert.That(state.Enabled, Is.False);
            Assert.That(state.Status, Is.EqualTo(ScriptLiveStatus.Failed));
        }

        [TestCase(null)]
        [TestCase("")]
        [TestCase("SCRIPT_STATE")]
        [TestCase("SCRIPT_STATE a.lua")]
        [TestCase("SCRIPT_STATE a.lua true")]
        [TestCase("SCRIPT_STATE a.lua yes running")]
        [TestCase("SCRIPT_STATE a.lua true waiting")]
        [TestCase("SCRIPT_SYNCED 3")]
        [TestCase("SCRIPT_STATEX a.lua true running")]
        public void AnythingElseIsNotAState(string line)
        {
            Assert.That(ScriptWire.TryParseState(line, out var state), Is.False);
            Assert.That(state, Is.Null);
        }

        [Test]
        public void WritesTheOrderPercentEncoded()
        {
            Assert.That(ScriptWire.Order(new[] { "my script.lua", "c.lua" }), Is.EqualTo("SCRIPT_ORDER my%20script.lua c.lua"));
            Assert.That(ScriptWire.Order(new string[0]), Is.EqualTo("SCRIPT_ORDER"));
        }

        [Test]
        public void ReadsTheOrderThePluginSends()
        {
            // The exact line TweakerPlugin/harness/remote expects the plugin to send.
            Assert.That(ScriptWire.TryParseOrder("SCRIPT_ORDER my%20script.lua c.lua d.lua", out var order), Is.True);
            Assert.That(order, Is.EqualTo(new[] { "my script.lua", "c.lua", "d.lua" }));

            Assert.That(ScriptWire.TryParseOrder("SCRIPT_ORDER", out order), Is.True);
            Assert.That(order, Is.Empty);
        }

        [TestCase(null)]
        [TestCase("")]
        [TestCase("SCRIPT_ORDERS a.lua")]
        [TestCase("SCRIPT_SYNCED 3")]
        public void AnythingElseIsNotAnOrder(string line)
        {
            Assert.That(ScriptWire.TryParseOrder(line, out var order), Is.False);
            Assert.That(order, Is.Null);
        }

        [Test]
        public void ReadsTheEndOfTheList()
        {
            Assert.That(ScriptWire.TryParseSynced("SCRIPT_SYNCED 3", out var count), Is.True);
            Assert.That(count, Is.EqualTo(3));
            Assert.That(ScriptWire.TryParseSynced("SCRIPT_SYNCED 0", out count), Is.True);
            Assert.That(count, Is.Zero);

            Assert.That(ScriptWire.TryParseSynced("SCRIPT_SYNCED", out _), Is.False);
            Assert.That(ScriptWire.TryParseSynced("SCRIPT_SYNCED -1", out _), Is.False);
            Assert.That(ScriptWire.TryParseSynced("SCRIPT_STATE a.lua true running", out _), Is.False);
        }
    }
}
