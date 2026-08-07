namespace QuickPlayerCore.Tests
{
    using System.Text.Json;
    using NUnit.Framework;

    // Playlists are files users already have on disk, so the two format changes Фаза 6 made - AutoAdvance
    // replaced by Mode, and the enums written by name instead of by ordinal - have to be non-events for
    // anything saved before them. Deserialised directly rather than through Playlist.Load, which would
    // reach into the real QuickPlayer app-data folder.
    [TestFixture]
    public class PlaylistSerializationTests
    {
        [Test]
        public void APlaylistSavedBeforeModeExistedKeepsBehavingTheSame()
        {
            const string json = """
                {
                  "Id": "1e8b9d2c-0000-4000-8000-000000000001",
                  "Name": "Old",
                  "Entries": [],
                  "AutoAdvance": true
                }
                """;

            var playlist = JsonSerializer.Deserialize<Playlist>(json);

            // The dropped field is ignored rather than fatal, and the mode it used to mean is the default.
            Assert.AreEqual(PlaybackMode.Sequential, playlist.Mode);
            Assert.AreEqual(AdvanceTrigger.SongComplete, playlist.AdvanceOn);
        }

        // AdvanceOn shipped serialized as a number, so the ordinals of that enum are part of files in the
        // wild. Reading numbers has to keep working now that both enums are written by name.
        [Test]
        public void EnumsWrittenAsNumbersStillRead()
        {
            const string json = """
                {
                  "Id": "1e8b9d2c-0000-4000-8000-000000000002",
                  "Name": "Numeric",
                  "Entries": [],
                  "AdvanceOn": 1
                }
                """;

            var playlist = JsonSerializer.Deserialize<Playlist>(json);

            Assert.AreEqual(AdvanceTrigger.CharacterScreen, playlist.AdvanceOn);
        }

        [Test]
        public void ModesRoundTripByName()
        {
            var playlist = new Playlist
            {
                Name = "New",
                Mode = PlaybackMode.ShuffleLoop,
                AdvanceOn = AdvanceTrigger.CharacterScreen
            };

            var json = JsonSerializer.Serialize(playlist);

            // By name, so inserting a mode later cannot renumber what everyone already has saved.
            Assert.That(json, Does.Contain("\"Mode\":\"ShuffleLoop\""));

            var restored = JsonSerializer.Deserialize<Playlist>(json);
            Assert.AreEqual(PlaybackMode.ShuffleLoop, restored.Mode);
            Assert.AreEqual(AdvanceTrigger.CharacterScreen, restored.AdvanceOn);
        }
    }
}
