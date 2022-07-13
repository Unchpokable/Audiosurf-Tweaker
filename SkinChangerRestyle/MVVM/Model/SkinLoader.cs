using System.Collections.ObjectModel;
using System.IO;
using System.Threading.Tasks;
using ChangerAPI;
using ChangerAPI.Utilities;
using ChangerAPI.Engine;

namespace SkinChangerRestyle.MVVM.Model
{
    internal class SkinLoader
    {
        public SkinLoader()
        {
            LoadedSkins = new ObservableCollection<SkinCard>();
        }

        public ObservableCollection<SkinCard> LoadedSkins { get; private set; }

        public async void LoadAllAsync(string path)
        {
            await Task.Run(() => LoadAll(path));
        }

        private void LoadAll(string path)
        {
            foreach (var file in Directory.GetFiles(path))
            {
                
            }
        }
    }
}
