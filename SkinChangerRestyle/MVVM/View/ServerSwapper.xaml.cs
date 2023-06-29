using ICSharpCode.AvalonEdit.CodeCompletion;
using ICSharpCode.AvalonEdit.Highlighting;
using ICSharpCode.AvalonEdit.Highlighting.Xshd;
using SkinChangerRestyle.MVVM.Model;
using System;
using System.Collections.Generic;
using System.IO;
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
using System.Xml;

namespace SkinChangerRestyle.MVVM.View
{
    /// <summary>
    /// Логика взаимодействия для ServerSwapper.xaml
    /// </summary>
    public partial class ServerSwapper : UserControl
    {
        public ServerSwapper()
        {
            InitializeComponent();

            IHighlightingDefinition customHightlight;

            using (var stream = new FileStream("TweakerScriptsSyntax.xshd", FileMode.Open))
            {
                using (var reader = new XmlTextReader(stream))
                {
                    customHightlight = HighlightingLoader.Load(reader, HighlightingManager.Instance);
                }
            }

            ScriptsEditor.SyntaxHighlighting = customHightlight;
        }
    }
}
