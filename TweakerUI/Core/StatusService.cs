using System;
using System.Collections.ObjectModel;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;

namespace TweakerUI.Core
{
    // Icon/visual-role selector for a StatusEntry - StatusBarStyles.axaml maps each value to a
    // StreamGeometry + accent/danger color via Classes selectors (see StatusEntry.Token).
    public enum StatusToken
    {
        Working,
        DiskProcess,
        Network,
        Playing,
        Warning,
        Error
    }

    // One chip in the status bar. Token/Message are mutated in place by StatusHandle.Update rather
    // than replacing the entry in ActiveStatuses, so a chip doesn't flicker/reorder in the bound
    // ItemsControl every time its text changes mid-operation.
    public sealed partial class StatusEntry : ObservableObject
    {
        public StatusEntry(StatusToken token, string source, string message)
        {
            Token = token;
            Source = source;
            Message = message;
        }

        public string Source { get; }

        [ObservableProperty]
        private StatusToken token;

        [ObservableProperty]
        private string message;
    }

    // Returned by StatusService.Begin - owns one StatusEntry for the lifetime of an operation.
    // Disposing removes the entry from the bar, so `using var status = StatusService.Manager.Begin(...)`
    // at the top of a method clears the status on every exit path (including early returns) without
    // each call site having to remember to reset it manually.
    public sealed class StatusHandle : IDisposable
    {
        private readonly StatusEntry _entry;
        private readonly Action<StatusEntry> _onDispose;
        private bool _disposed;

        internal StatusHandle(StatusEntry entry, Action<StatusEntry> onDispose)
        {
            _entry = entry;
            _onDispose = onDispose;
        }

        public void Update(string message) => _entry.Message = message;

        public void Update(StatusToken token, string message)
        {
            _entry.Token = token;
            _entry.Message = message;
        }

        public void Dispose()
        {
            if (_disposed)
                return;

            _disposed = true;
            _onDispose(_entry);
        }
    }

    // App-wide, multi-source status bar service - replaces SkinChangerViewModel's old ChangerStatus
    // (a property that was updated in 8 places but never actually bound to any View). Any number of
    // services can have an active entry at once (e.g. Skin Changer installing a skin while Quick
    // Player is mid-playback) - each just owns its own StatusHandle, independent of the others.
    internal class StatusService
    {
        private StatusService() { }

        public static StatusService Manager => _instance ??= new StatusService();

        private static StatusService _instance;

        public ObservableCollection<StatusEntry> ActiveStatuses { get; } = new();

        public StatusHandle Begin(StatusToken token, string source, string message)
        {
            var entry = new StatusEntry(token, source, message);
            Dispatcher.UIThread.Post(() => ActiveStatuses.Add(entry));
            return new StatusHandle(entry, Remove);
        }

        private void Remove(StatusEntry entry) => Dispatcher.UIThread.Post(() => ActiveStatuses.Remove(entry));
    }
}
