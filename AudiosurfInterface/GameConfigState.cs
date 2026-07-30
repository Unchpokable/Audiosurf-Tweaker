using System;
using System.Collections.Generic;
using System.Linq;

namespace AudiosurfInterface
{
    public enum GameConfigOverrideSource
    {
        None,
        Temporary,
        QuickPlayer
    }

    public readonly record struct GameConfigSnapshot(
        string Key,
        bool GlobalValue,
        bool EffectiveValue,
        GameConfigOverrideSource OverrideSource)
    {
        public bool HasOverride => OverrideSource != GameConfigOverrideSource.None;
    }

    public sealed class GameConfigStateChangedEventArgs : EventArgs
    {
        public GameConfigStateChangedEventArgs(GameConfigSnapshot snapshot)
        {
            Snapshot = snapshot;
        }

        public GameConfigSnapshot Snapshot { get; }
    }

    /// <summary>
    /// Single source of truth for global and effective asconfig state. The game cannot report config
    /// values back, so temporary Quick Player overrides and permanent user changes are tracked
    /// separately here and every resulting game command is emitted from this one place.
    /// </summary>
    public class GameConfigState
    {
        private GameConfigState() { }

        public static GameConfigState Manager => _instance ??= new GameConfigState();

        public event EventHandler<GameConfigStateChangedEventArgs> StateChanged;

        private static GameConfigState _instance;

        private readonly Dictionary<string, ConfigEntry> _entries = new(StringComparer.Ordinal);
        private readonly object _lock = new();
        private long _nextOverrideId;

        public void Set(string key, bool value)
        {
            UpdateGlobal(key, value, clearOverrides: false);
        }

        /// <summary>
        /// Commits a value as global and cancels every temporary override on the key. Used for an
        /// in-game overlay click: that toggle displays the effective value, so clicking it means
        /// "make the newly displayed value permanent now", not "edit a hidden baseline".
        /// </summary>
        public void SetAndClearOverrides(string key, bool value)
        {
            UpdateGlobal(key, value, clearOverrides: true);
        }

        /// <summary>
        /// Temporarily overrides a key. Dispose removes this exact stack entry and recomputes the
        /// effective value from any remaining overrides plus the latest global baseline.
        /// </summary>
        public IDisposable PushOverride(
            string key,
            bool value,
            GameConfigOverrideSource source = GameConfigOverrideSource.Temporary)
        {
            ConfigOverride activeOverride;
            GameConfigSnapshot before;
            GameConfigSnapshot after;

            lock (_lock)
            {
                var entry = GetOrCreateEntryLocked(key);
                before = SnapshotLocked(key, entry);

                activeOverride = new ConfigOverride(++_nextOverrideId, value, source);
                entry.Overrides.Add(activeOverride);
                after = SnapshotLocked(key, entry);
            }

            PublishTransition(before, after);
            return new ConfigOverrideHandle(this, key, activeOverride.Id);
        }

        public GameConfigSnapshot GetSnapshot(string key)
        {
            lock (_lock)
            {
                var entry = GetOrCreateEntryLocked(key);
                return SnapshotLocked(key, entry);
            }
        }

        public IReadOnlyList<GameConfigSnapshot> GetKnownTweakSnapshots()
        {
            lock (_lock)
            {
                return GameTweakCatalog.All
                    .Select(definition =>
                    {
                        var entry = GetOrCreateEntryLocked(definition.ConfigKey);
                        return SnapshotLocked(definition.ConfigKey, entry);
                    })
                    .ToArray();
            }
        }

        private void UpdateGlobal(string key, bool value, bool clearOverrides)
        {
            GameConfigSnapshot before;
            GameConfigSnapshot after;

            lock (_lock)
            {
                var entry = GetOrCreateEntryLocked(key);
                before = SnapshotLocked(key, entry);
                entry.GlobalValue = value;

                // A desktop/global change matching the currently effective temporary value promotes
                // that value: the override is redundant and must not restore an older baseline later.
                // The explicit overlay path clears regardless of whether the values match.
                if(clearOverrides || (entry.Overrides.Count > 0 && before.EffectiveValue == value))
                    entry.Overrides.Clear();

                after = SnapshotLocked(key, entry);
            }

            PublishTransition(before, after);
        }

        private void RemoveOverride(string key, long overrideId)
        {
            GameConfigSnapshot before;
            GameConfigSnapshot after;
            var removed = false;

            lock (_lock)
            {
                if (!_entries.TryGetValue(key, out var entry))
                    return;

                before = SnapshotLocked(key, entry);
                var index = entry.Overrides.FindIndex(item => item.Id == overrideId);
                if (index >= 0)
                {
                    entry.Overrides.RemoveAt(index);
                    removed = true;
                }
                after = SnapshotLocked(key, entry);
            }

            if (removed)
                PublishTransition(before, after);
        }

        private ConfigEntry GetOrCreateEntryLocked(string key)
        {
            if (!_entries.TryGetValue(key, out var entry))
            {
                entry = new ConfigEntry { GlobalValue = GameTweakCatalog.DefaultConfigValue(key) };
                _entries.Add(key, entry);
            }

            return entry;
        }

        private static GameConfigSnapshot SnapshotLocked(string key, ConfigEntry entry)
        {
            if (entry.Overrides.Count == 0)
                return new GameConfigSnapshot(key, entry.GlobalValue, entry.GlobalValue, GameConfigOverrideSource.None);

            var activeOverride = entry.Overrides[entry.Overrides.Count - 1];
            return new GameConfigSnapshot(key, entry.GlobalValue, activeOverride.Value, activeOverride.Source);
        }

        private void PublishTransition(GameConfigSnapshot before, GameConfigSnapshot after)
        {
            if (before.EffectiveValue != after.EffectiveValue)
                AudiosurfHandle.Instance.Command(GameProtocol.Config(after.Key, after.EffectiveValue));

            if (before != after)
                StateChanged?.Invoke(this, new GameConfigStateChangedEventArgs(after));
        }

        private sealed class ConfigEntry
        {
            public bool GlobalValue;
            public List<ConfigOverride> Overrides { get; } = new();
        }

        private sealed class ConfigOverride
        {
            public ConfigOverride(long id, bool value, GameConfigOverrideSource source)
            {
                Id = id;
                Value = value;
                Source = source;
            }

            public long Id { get; }
            public bool Value { get; }
            public GameConfigOverrideSource Source { get; }
        }

        private sealed class ConfigOverrideHandle : IDisposable
        {
            private readonly GameConfigState _owner;
            private readonly string _key;
            private readonly long _overrideId;
            private bool _disposed;

            public ConfigOverrideHandle(GameConfigState owner, string key, long overrideId)
            {
                _owner = owner;
                _key = key;
                _overrideId = overrideId;
            }

            public void Dispose()
            {
                if (_disposed)
                    return;

                _disposed = true;
                _owner.RemoveOverride(_key, _overrideId);
            }
        }
    }
}
