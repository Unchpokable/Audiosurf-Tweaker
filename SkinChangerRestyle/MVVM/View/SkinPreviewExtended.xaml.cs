namespace SkinChangerRestyle.MVVM.View
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;
    using System.Windows;
    using System.Windows.Controls;
    using System.Windows.Data;
    using System.Windows.Documents;
    using System.Windows.Input;
    using System.Windows.Media;
    using System.Windows.Media.Imaging;
    using System.Windows.Navigation;
    using System.Windows.Shapes;
    using ChangerAPI.Engine;
    /// <summary>
    /// Логика взаимодействия для SkinPreviewExtended.xaml
    /// </summary>
    public partial class SkinPreviewExtended : UserControl
    {
        private AudiosurfSkin currentSkin;

        public SkinPreviewExtended()
        {
            InitializeComponent();
        }

        public bool AssignSkin(AudiosurfSkin skin)
        {
            if (skin == null) return false;
            currentSkin = skin;
            return true;
        }
    }
}
