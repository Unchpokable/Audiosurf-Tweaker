using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SkiaSharp;
using TweakerCore;
using TweakerCore.Engine;
using TweakerUI.Core;
using TweakerUI.Core.Utils;
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
        public SkinCard(AudiosurfSkinExtended skin, string pathToOrigin, SkinChangerViewModel root = null)
        {
            _root = root;
            AssignSkin(skin, pathToOrigin);
        }

        internal SkinCard(LoadedSkinData skin, SkinChangerViewModel root = null)
        {
            _root = root;
            AssignSkin(skin);
        }

        private readonly SkinChangerViewModel _root;
        private string _pathToOriginFile;

        [ObservableProperty]
        private string name;

        [ObservableProperty]
        private ObservableCollection<InteractableScreenshot> screenshots;

        [ObservableProperty]
        private bool renameActive;

        // Pre-filled with the current name when entering rename mode (rather than starting blank) -
        // matches the "rename a file" mental model instead of forcing the user to retype it.
        [ObservableProperty]
        private string newName;

        public bool UseFastPreview => SettingsProvider.UseFastPreview;

        public string InstallTooltip => "Clear Installation";
        public string ExportCopyTooltip => "Export copy of this skin";
        public string RenameTooltip => "Rename this skin";
        public string RemoveTooltip => "Remove this skin";

        public string PathToOrigin => _pathToOriginFile;

        private void AssignSkin(AudiosurfSkinExtended skin, string pathToOrigin)
        {
            if (skin == null) return;

            _pathToOriginFile = pathToOrigin;
            Name = skin.Name;
            if (UseFastPreview)
                Screenshots = new ObservableCollection<InteractableScreenshot>(
                    (skin.Previews?.Group ?? System.Array.Empty<NamedBitmap>())
                    .Select(screenshot => new InteractableScreenshot(((SKBitmap)screenshot).Rescale(860, 440).ToAvaloniaBitmap())));
        }

        private void AssignSkin(LoadedSkinData skin)
        {
            if (skin == null) return;

            _pathToOriginFile = skin.PathToOriginFile;
            Name = skin.Name;
            if (UseFastPreview)
                Screenshots = new ObservableCollection<InteractableScreenshot>(
                    skin.Screenshots.Select(x => new InteractableScreenshot(x.ToAvaloniaBitmap())));
        }

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
            var oldName = Name;
            var newName = NewName;
            RenameActive = false;

            if (string.IsNullOrWhiteSpace(newName) || newName == oldName)
                return;

            var skinObject = SkinPackager.Decompile(_pathToOriginFile);
            if (skinObject == null)
                return;

            skinObject.Name = newName;
            var newFile = $@"Skins\{newName}{EnvironmentalVeriables.ActualSkinExtention}";
            var oldFile = _pathToOriginFile;

            // Name/_pathToOriginFile are only committed below once the file operations actually
            // succeed - previously they were updated optimistically up front, so a failed compile or a
            // locked/undeletable old file left the card pointing at a file that didn't exist on disk.
            var success = await Task.Run(() =>
            {
                try
                {
                    if (!SkinPackager.CompileToPath(skinObject, "Skins"))
                        return false;

                    File.Delete(oldFile);

                    var cacheDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
                    if (LoadingCache.TryFind(cacheDir, out LoadingCache cache))
                    {
                        cache.Data.RemoveAll(x => x.Name == oldName);
                        cache.Data.Add(new LoadedSkinData(skinObject, newFile));
                        cache.Serialize(cacheDir);
                        Utils.DisposeAndClear(cache);
                    }

                    return true;
                }
                catch
                {
                    return false;
                }
            });

            Utils.DisposeAndClear(skinObject);

            if (success)
            {
                Name = newName;
                _pathToOriginFile = newFile;
            }
            else
            {
                ApplicationNotificationManager.Manager.ShowError("Rename failed", $"Could not rename skin \"{oldName}\" to \"{newName}\". The skin file on disk was left unchanged.");
            }
        }

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
