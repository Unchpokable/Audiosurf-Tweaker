using CommunityToolkit.Mvvm.ComponentModel;
using TweakerCore.Skyboxes;

namespace TweakerUI.Models
{
    public partial class SkyRowViewModel : ObservableObject
    {
        public SkyRowViewModel(SkyCatalogEntry entry, string path, bool shipped, string project)
        {
            Entry = entry;
            FullPath = path;
            IsShipped = shipped;
            ProjectName = project;
            Origin = !SkyKinds.IsOnDisk(entry.Kind) ? "Built-in" : shipped ? "Bundled"
                : project != null ? "From project " + project : "Yours";
        }

        public SkyCatalogEntry Entry { get; }
        public string FullPath { get; }
        public bool IsShipped { get; }
        public string ProjectName { get; }
        public string Origin { get; }
        public string Title => Entry.DisplayName;
        public string Description => Entry.Kind switch
        {
            SkyKind.Program => "built-in shader",
            SkyKind.Packed => "built-in image",
            SkyKind.ShaderFile => "shader file",
            SkyKind.Package => "sky package",
            SkyKind.FaceDir => "cube faces",
            _ => "cube map"
        } + (Entry.FaceSize > 0 ? $" · {Entry.FaceSize} px" : "") + " · " + Origin;

        public bool CanReveal => FullPath != null;
        public bool CanDelete => CanReveal && !IsShipped;

        [ObservableProperty]
        private bool isActive;
    }
}
