using System;
using System.Collections.Generic;

namespace AudiosurfInterface
{
    /// <summary>
    /// Single source of truth for "last known value sent for an asconfig key" - the game gives no
    /// way to read its current config back, so anything that needs to temporarily override a key and
    /// later restore it (QuickPlayer's per-song tweaks) has to track that value itself. Every
    /// asconfig send - global Tweaker toggles included - should go through here rather than straight
    /// through AudiosurfHandle.Command, so overrides always restore to what the user actually set.
    /// </summary>
    public class GameConfigState
    {
        private GameConfigState() { }

        public static GameConfigState Manager => _instance ??= new GameConfigState();

        private static GameConfigState _instance;

        private readonly Dictionary<string, bool> _lastKnownValues = new();
        private readonly object _lock = new();

        public void Set(string key, bool value)
        {
            lock (_lock)
                _lastKnownValues[key] = value;

            AudiosurfHandle.Instance.Command(GameProtocol.Config(key, value));
        }

        /// <summary>
        /// Temporarily overrides a key, restoring the value it had immediately before this call on
        /// Dispose. If nobody has ever set the key before, the game's own untouched-tweak default is
        /// "off" - every asconfig key here is a boolean toggle the game itself defaults to false until
        /// something explicitly turns it on - so an unknown key restores to false rather than being
        /// left at whatever the override set it to.
        /// </summary>
        public IDisposable PushOverride(string key, bool value)
        {
            bool previous;

            lock (_lock)
                _lastKnownValues.TryGetValue(key, out previous); // false when the key was never set - the game's own default

            Set(key, value);
            return new ConfigOverrideHandle(this, key, previous);
        }

        private sealed class ConfigOverrideHandle : IDisposable
        {
            private readonly GameConfigState _owner;
            private readonly string _key;
            private readonly bool _previous;
            private bool _disposed;

            public ConfigOverrideHandle(GameConfigState owner, string key, bool previous)
            {
                _owner = owner;
                _key = key;
                _previous = previous;
            }

            public void Dispose()
            {
                if (_disposed)
                    return;

                _disposed = true;
                _owner.Set(_key, _previous);
            }
        }
    }
}
