using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Media.Imaging;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerCore;
using TweakerCore.Engine;
using TweakerUI.Core;
using TweakerUI.Services;
using TweakerUI.ViewModels;

namespace TweakerUI.Models
{
    // Ported from SkinChangerRestyle.MVVM.Model.SkinCard - EditOnDisk is dropped entirely (the old
    // app's "Edit on Disk" mode is retired for this revival; a replacement is planned separately, not
    // part of this port). Icon ImageSource properties are gone too: the new view draws each action's
    // icon straight from a shared Geometry resource in Styles.axaml instead of a per-card property.
    public partial class SkinCard : ObservableObject
    {
        // Name and file only - screenshots are never decoded here (see Screenshots).
        public SkinCard(string name, string pathToOrigin, SkinChangerViewModel root = null)
        {
            _root = root;
            Name = name;
            _pathToOriginFile = pathToOrigin;
        }

        private readonly SkinChangerViewModel _root;
        private string _pathToOriginFile;

        [ObservableProperty]
        private string name;

        // Non-null only in Fast Previews mode: filled in the background by SkinChangerViewModel after the
        // list loads or when the mode is switched on, dropped (and disposed) when it's switched off.
        // Whoever shows screenshots checks this, not the setting - the two only agree once a runtime
        // switch has finished filling/clearing every card.
        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(Cover))]
        private ObservableCollection<InteractableScreenshot> screenshots;

        // Hover preview on the card (the WPF version's ToolTip, restored). Null outside Fast Previews
        // mode, which also disables the tooltip - it never touches the disk.
        public Bitmap Cover => Screenshots?.FirstOrDefault()?.Image;

        [ObservableProperty]
        private bool renameActive;

        // Pre-filled with the current name when entering rename mode (rather than starting blank) -
        // matches the "rename a file" mental model instead of forcing the user to retype it.
        [ObservableProperty]
        private string newName;

        public string InstallTooltip => "Clear Installation";
        public string ExportCopyTooltip => "Export copy of this skin";
        public string RenameTooltip => "Rename this skin";
        public string RemoveTooltip => "Remove this skin";

        public string PathToOrigin => _pathToOriginFile;

        [RelayCommand]
        private void Install() => _root?.InstallSkin(_pathToOriginFile, SettingsProvider.GameTexturesPath, forced: true, clearInstall: true);

        [RelayCommand]
        private void EnableRename()
        {
            NewName = Name;
            RenameActive = true;
        }

        [RelayCommand]
        private void CancelRename() => RenameActive = false;

        [RelayCommand]
        private async Task ApplyRename()
        {
            if (_renameInProgress)
                return;

            var oldName = Name;
            var newName = NewName?.Trim();

            if (string.IsNullOrEmpty(newName) || newName == oldName)
            {
                RenameActive = false;
                return;
            }

            // Validation failures keep rename mode open, so the user can fix the name instead of
            // retyping it from scratch.
            if (newName.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
            {
                ApplicationNotificationManager.Manager.ShowError("Rename failed", $"\"{newName}\" contains characters that can't be used in a file name.");
                return;
            }

            // The skin stays in its own folder - skins from the additional skins folder used to be
            // silently moved into Skins\ by a rename.
            var oldFile = _pathToOriginFile;
            var newFile = Path.Combine(Path.GetDirectoryName(oldFile), newName + EnvironmentalVeriables.ActualSkinExtention);
            var samePath = string.Equals(Path.GetFullPath(oldFile), Path.GetFullPath(newFile), StringComparison.OrdinalIgnoreCase);

            // The name is what the overlay and the installed-skin state identify a skin by, and an
            // existing file would simply be overwritten - both used to turn two skins into one.
            if ((!samePath && File.Exists(newFile)) || (_root?.Skins.Any(c => c != this && c.Name == newName) ?? false))
            {
                ApplicationNotificationManager.Manager.ShowError("Rename failed", $"A skin named \"{newName}\" already exists.");
                return;
            }

            // Name is shown right away and rolled back on failure; _pathToOriginFile is committed only
            // on success, so the card never points at a file that isn't on disk.
            RenameActive = false;
            Name = newName;
            _renameInProgress = true;

            bool success;
            using (StatusService.Manager.Begin(StatusToken.DiskProcess, "Skin Changer", $"Renaming \"{oldName}\" to \"{newName}\"..."))
            {
                success = await Task.Run(() => SkinPackager.RenameSkin(oldFile, newName, newFile));
            }

            _renameInProgress = false;

            if (success)
            {
                _pathToOriginFile = newFile;
                _root?.OnSkinRenamed(oldName, newName);
            }
            else
            {
                Name = oldName;
                ApplicationNotificationManager.Manager.ShowError("Rename failed", $"Could not rename skin \"{oldName}\" to \"{newName}\". The skin file on disk was left unchanged.");
            }
        }

        private bool _renameInProgress;

        [RelayCommand]
        private async Task ExportCopy()
        {
            var folder = await FileDialogService.OpenFolderAsync("Select export destination");
            if (folder == null)
                return;

            try
            {
                var targetFile = Path.Combine(folder, Name + EnvironmentalVeriables.ActualSkinExtention);
                await Task.Run(() => File.Copy(_pathToOriginFile, targetFile));
                ApplicationNotificationManager.Manager.ShowSuccess("Done!", "Copy of skin file successfully exported to selected destination");
            }
            catch
            {
                ApplicationNotificationManager.Manager.ShowError("Oooops!", "Something went wrong! Please, check that destination path contains only latin symbols and you're trying to export valid skin and try again");
            }
        }

        [RelayCommand]
        private void Remove() => _root?.RemoveSkin(this);
    }
}
