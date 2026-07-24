namespace AudiosurfInterface.Tests
{
    using AudiosurfInterface.Bridge;
    using NUnit.Framework;

    [TestFixture]
    public class AsBridgeProtocolTests
    {
        // SerializeSend/SerializeOverlaySend (client -> server) and TryParseReport (server -> client)
        // share the same escaped, double-quoted, whitespace-separated "details" grammar regardless of
        // header/msg token - swapping the client-command header for a server-report one round-trips a
        // serialized command through the exact same escape/unescape pair TryParseReport itself uses,
        // without needing to touch either method's private internals.
        private static string AsServerReport(string serializedClientCommand)
        {
            return serializedClientCommand
                .Replace("CCOMMAND SEND", "SREPORT OK")
                .Replace("CCOMMAND OVERLAY_SEND", "SREPORT OK");
        }

        [TestCase("ascommand reloadtextures")]
        [TestCase("path with \"embedded quotes\" in it")]
        [TestCase(@"C:\Users\test\Songs\track.mp3")]
        [TestCase(@"C:\Users\test\Songs\""weird""\track.mp3")]
        public void SerializeSend_RoundTripsThroughTryParseReport(string command)
        {
            var wire = AsBridgeProtocol.SerializeSend(command);

            var parsed = AsBridgeProtocol.TryParseReport(AsServerReport(wire), out var report);

            Assert.IsTrue(parsed);
            Assert.AreEqual(1, report.Details.Count);
            Assert.AreEqual(command, report.Details[0]);
        }

        [TestCase("TWEAK_SET roadvisible true")]
        [TestCase("payload with \\ and \" both present")]
        public void SerializeOverlaySend_RoundTripsThroughTryParseReport(string payload)
        {
            var wire = AsBridgeProtocol.SerializeOverlaySend(payload);

            var parsed = AsBridgeProtocol.TryParseReport(AsServerReport(wire), out var report);

            Assert.IsTrue(parsed);
            Assert.AreEqual(1, report.Details.Count);
            Assert.AreEqual(payload, report.Details[0]);
        }

        [Test]
        public void TryParseReport_ParsesServiceReportWithMultipleDetails()
        {
            var parsed = AsBridgeProtocol.TryParseReport(
                $"SREPORT SERVICE \"{AsBridgeProtocol.ServiceStatusWindowFound}\" \"1234\" ", out var report);

            Assert.IsTrue(parsed);
            Assert.AreEqual(AsBridgeReportType.Service, report.Type);
            Assert.That(report.Details, Is.EqualTo(new[] { AsBridgeProtocol.ServiceStatusWindowFound, "1234" }));
        }

        [TestCase("")]
        [TestCase("SREPORT")]
        [TestCase("SREPORT OK unterminated \"quote")]
        [TestCase("NOT_SREPORT OK \"detail\" ")]
        public void TryParseReport_RejectsMalformedInput(string raw)
        {
            Assert.IsFalse(AsBridgeProtocol.TryParseReport(raw, out _));
        }
    }
}
