using System.Collections.ObjectModel;
using System.IO;
using CommunityToolkit.Mvvm.ComponentModel;
using TweakerCore.Skyboxes;

namespace TweakerUI.Models
{
    public partial class SkyProjectRowViewModel : ObservableObject
    {
        public SkyProjectRowViewModel(SkyProject project) => Project = project;

        public SkyProject Project { get; private set; }
        public string Root => Project.Root;
        public string Title => Project.Name ?? Path.GetFileName(Path.TrimEndingDirectorySeparator(Root));
        public string PackageFileName => Project.PackageFileName;
        public string Details => (PackageFileName == null ? "No package name" : "Installs as " + PackageFileName)
            + (Project.Version != null ? " · v" + Project.Version : "")
            + (!string.IsNullOrEmpty(Project.Author) ? " · by " + Project.Author : "");
        public string Problem => Project.ManifestProblem ?? Project.NameProblem;
        public bool IsMissing => Project.IsMissing;
        public bool CanForget => !Project.InSources;

        public ObservableCollection<SkyProblem> Problems { get; } = new ObservableCollection<SkyProblem>();

        [ObservableProperty]
        private string result;

        public void Update(SkyProject project)
        {
            Project = project;
            OnPropertyChanged(string.Empty);
        }
    }
}
