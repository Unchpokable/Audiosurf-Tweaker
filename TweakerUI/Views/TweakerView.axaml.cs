using Avalonia.Controls;

namespace TweakerUI.Views
{
    public partial class TweakerView : UserControl
    {
        public TweakerView()
        {
            InitializeComponent();
        }

        // Text is bound one-way from ConsoleContent, so every new line replaces the whole string;
        // pushing the caret to the end brings it (and the scroll position) along with it.
        private void OnConsoleTextChanged(object sender, TextChangedEventArgs e)
        {
            if (sender is TextBox textBox)
                textBox.CaretIndex = textBox.Text?.Length ?? 0;
        }
    }
}
