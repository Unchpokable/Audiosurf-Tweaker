using Audiosurf_SkinChanger.Engine;
using System.Windows.Forms;
using System;

namespace SkinCreator
{
    public partial class SkinCreationForm : Form
    {
        internal event Action Callback;
        public SkinCreationForm(object parent, Type parentType, Action callback)
        {
            InitializeComponent();
            Callback += callback;
        }

        public SkinCreationForm()
        {
            InitializeComponent();
        }
    }
}
