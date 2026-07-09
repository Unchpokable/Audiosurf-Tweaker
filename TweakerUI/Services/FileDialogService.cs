using System.Collections.Generic;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Platform.Storage;
using TweakerUI.Core;

namespace TweakerUI.Services
{
    // Replaces the WinForms OpenFileDialog/FolderBrowserDialog/SaveFileDialog calls the old view models
    // made directly. Avalonia has no synchronous file dialog API - everything goes through the owning
    // TopLevel's IStorageProvider and is async, so every call site that used to check
    // "dialog.ShowDialog() == DialogResult.OK" becomes an awaited call here instead.
    internal static class FileDialogService
    {
        public static async Task<string> OpenFileAsync(string title, IReadOnlyList<FilePickerFileType> fileTypes = null, string suggestedStartLocation = null)
        {
            var window = AppShell.MainWindow;
            if (window == null) return null;

            var startFolder = await ResolveStartFolderAsync(window, suggestedStartLocation);
            var result = await window.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
            {
                Title = title,
                AllowMultiple = false,
                FileTypeFilter = fileTypes,
                SuggestedStartLocation = startFolder,
            });

            return result.Count > 0 ? result[0].TryGetLocalPath() : null;
        }

        public static async Task<string> OpenFolderAsync(string title)
        {
            var window = AppShell.MainWindow;
            if (window == null) return null;

            var result = await window.StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
            {
                Title = title,
                AllowMultiple = false,
            });

            return result.Count > 0 ? result[0].TryGetLocalPath() : null;
        }

        public static async Task<string> SaveFileAsync(string title, string suggestedFileName = null, string suggestedStartLocation = null)
        {
            var window = AppShell.MainWindow;
            if (window == null) return null;

            var startFolder = await ResolveStartFolderAsync(window, suggestedStartLocation);
            var result = await window.StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
            {
                Title = title,
                SuggestedFileName = suggestedFileName,
                SuggestedStartLocation = startFolder,
            });

            return result?.TryGetLocalPath();
        }

        private static async Task<IStorageFolder> ResolveStartFolderAsync(Window window, string path)
        {
            return string.IsNullOrEmpty(path) ? null : await window.StorageProvider.TryGetFolderFromPathAsync(path);
        }
    }
}
