using ChangerAPI.Engine;
using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.Extensions;
using SkinChangerRestyle.MVVM.ViewModel;
using System;
using System.Collections.ObjectModel;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Forms;
using System.Windows.Media;

namespace SkinChangerRestyle.MVVM.Model
{
    class SkinCard : ObservableObject
    {
        public SkinCard(AudiosurfSkinExtended skin, string pathToOrigin, SkinChangerViewModel root = null)
        {
            _rootVM = root;
            AssignSkin(skin, pathToOrigin);
            InitializeFields();
        }

        public SkinCard(LoadedSkinData skin, SkinChangerViewModel root = null)
        {
            _rootVM = root;
            AssignSkin(skin);
            InitializeFields();
        }

        public bool IsRenameFocused { get; set; }


        public bool RenameActive
        {
            get => _renameActive;
            set
            {
                _renameActive = value;
                OnPropertyChanged();
            }
        }

        public Visibility RenameVisible
        {
            get => _renameVisible;
            set
            {
                _renameVisible = (Visibility)Enum.Parse(typeof(Visibility), value.ToString());
                OnPropertyChanged();
            }
        }


        public string NewName
        {
            get => _renameName;
            set
            {
                _renameName = value;
                OnPropertyChanged();
            }
        }

        public string Name
        {
            get => _name;
            set
            {
                _name = value;
                OnPropertyChanged();
            }
        }
        public ObservableCollection<InteractableScreenshot> Screenshots
        {
            get => _screenshots;
            set
            {
                _screenshots = value;
                OnPropertyChanged();
            }
        }

        public bool UseFastPreview => SettingsProvider.UseFastPreview;

        public ImageSource Cover => Screenshots?.FirstOrDefault()?.Image;

        public string InstallTooltip => "Clear Installation";
        public string ExportCopyTooltip => "Export copy of this skin";
        public string RenameTooltip => "Rename this skin";
        public string EditOnDiskTooltip => "Turn ASTweaker into EditOnDisk mode";
        public string RemoveToolTip => "Remove this skin";
        public string PathToOrigin => _pathToOriginFile;

        public ImageSource InstallIcon { get; set; }
        public ImageSource ExportCopyIcon { get; set; }
        public ImageSource RenameIcon { get; set; }
        public ImageSource EditOnDiskIcon { get; set; }
        public ImageSource RemoveIcon { get; set; }

        public RelayCommand RemoveCommand { get; set; }
        public RelayCommand InstallCommand { get; set; }
        public RelayCommand ExportCopyCommand { get; set; }
        public RelayCommand EditOnDiskCommand { get; set; }
        public RelayCommand EnableRename { get; set; }
        public RelayCommand ApplyRename { get; set; }


        private string _pathToOriginFile;
        private string _name;
        private string _renameName;
        private SkinChangerViewModel _rootVM;
        private ObservableCollection<InteractableScreenshot> _screenshots;
        private bool _renameActive;
        private Visibility _renameVisible;

        private void AssignSkin(AudiosurfSkinExtended skin, string pathToOrigin)
        {
            if (skin == null) return;

            _pathToOriginFile = pathToOrigin;
            Name = $"{skin.Name}";
            if (UseFastPreview)
                Screenshots = new ObservableCollection<InteractableScreenshot>(skin.Previews.Group.Select(screenshot => new InteractableScreenshot(((Bitmap)screenshot).Rescale(860, 440).ToImageSource())));
        }

        private void AssignSkin(LoadedSkinData skin)
        {
            if (skin == null) return;
            _pathToOriginFile = skin.PathToOriginFile;
            Name = skin.Name;
            if (UseFastPreview)
                Screenshots = new ObservableCollection<InteractableScreenshot>(skin.Screenshots.Select(x => new InteractableScreenshot(x.ToImageSource())));
        }

        private void Install(object frameworkRequieredParameter)
        {
            _rootVM.InstallSkin(_pathToOriginFile, SettingsProvider.GameTexturesPath, forced: true, clearInstall: true);
        }

        private void EnableRenameInternal(object frameworkRequieredParameter)
        {
            NewName = "";
            RenameVisible = Visibility.Visible;
            RenameActive = true;
            IsRenameFocused = true;
        }

        private async void ApplyRenameInternal(object frameworkRequieredParameter)
        {
            var oldName = Name;
            var newName = NewName;
            var skinObject = SkinPackager.Decompile(_pathToOriginFile);
            skinObject.Name = newName;
            Name = newName;
            var newFile = $@"Skins\{newName}{ChangerAPI.EnvironmentalVeriables.ActualSkinExtention}";
            var oldFile = $"{_pathToOriginFile}";
            _pathToOriginFile = newFile;
            RenameActive = false;
            RenameVisible = Visibility.Hidden;
            IsRenameFocused = false;
            await Task.Run(() =>
            {
                SkinPackager.CompileToPath(skinObject, "Skins");
                File.Delete(oldFile);

                if (LoadingCache.TryFind(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), out LoadingCache cache))
                {
                    cache.Data.RemoveAll(x => x.Name == oldName);
                    cache.Data.Add(new LoadedSkinData(skinObject, newFile));
                    cache.Serialize(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location));
                }

                Extensions.DisposeAndClear(cache, skinObject);
            });
        }

        private async void EditOnDisk(object frameworkRequieredParameter)
        {
            if (string.IsNullOrEmpty(_pathToOriginFile))
                return;
            string tempDirectory = Path.Combine(Path.GetTempPath(), Path.GetRandomFileName());
            Directory.CreateDirectory(tempDirectory);
            _rootVM.InstallSkin(_pathToOriginFile, tempDirectory, forced: true, unpackScreenshots: true, saveState: false);
            var dirproc = System.Diagnostics.Process.Start(tempDirectory);
            new EditOnDiskLockWindow().ShowDialog();
            var redactedSkin = SkinPackager.CreateSkinFromFolder(tempDirectory);

            if (redactedSkin == null)
                return;

            redactedSkin.Name = Name;
            await Task.Run(() =>
            {
                SkinPackager.RewriteCompile(redactedSkin, _pathToOriginFile);
                if (LoadingCache.TryFind(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), out LoadingCache cache))
                {
                    cache.Data.RemoveAll(x => x.Name == Name);
                    cache.Data.Add(new LoadedSkinData(redactedSkin, _pathToOriginFile));
                    cache.Serialize(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location));
                    cache.Dispose();
                }
            });

            AssignSkin(redactedSkin, _pathToOriginFile);
            Extensions.DisposeAndClear(dirproc, redactedSkin);

            try
            {
                Directory.Delete(tempDirectory, true);
            }
            catch (IOException)
            {
                System.Windows.MessageBox.Show($"Something went wrong while cleaning working directory at {tempDirectory}, so this directory still exists but its unused. Sorry for poop in your temp :)", "Ooops!", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void ExportCopyInternal(object frameworkRequieredParameter)
        {
            try
            {
                using (var output = new FolderBrowserDialog())
                    if (output.ShowDialog() == DialogResult.OK)
                    {
                        var skin = SkinPackager.Decompile(_pathToOriginFile);
                        SkinPackager.CompileToPath(skin, output.SelectedPath);
                        Extensions.DisposeAndClear(skin);
                    }
            }
            catch
            {
                System.Windows.MessageBox.Show("Something went wrong! Please, check that destination path contains only latin symbols and you're trying to export valid skin and try again", "Ooops!", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void RemoveSkinInternal(object obj)
        {
            _rootVM.RemoveSkin(this);
        }

        private void InitializeFields()
        {
            InstallIcon = Properties.Resources.install.ToImageSource();
            ExportCopyIcon = Properties.Resources.export.ToImageSource();
            RenameIcon = Properties.Resources.edit.ToImageSource();
            EditOnDiskIcon = Properties.Resources.editondisk.ToImageSource();
            RemoveIcon = Properties.Resources.trash.ToImageSource();
            _renameVisible = Visibility.Hidden;
            _renameActive = false;

            EnableRename = new RelayCommand(EnableRenameInternal);
            ApplyRename = new RelayCommand(ApplyRenameInternal);
            InstallCommand = new RelayCommand(Install);
            EditOnDiskCommand = new RelayCommand(EditOnDisk);
            ExportCopyCommand = new RelayCommand(ExportCopyInternal);
            RemoveCommand = new RelayCommand(RemoveSkinInternal);
        }
    }
}
