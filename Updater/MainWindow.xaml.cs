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

namespace Updater
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();

            var args = Environment.GetCommandLineArgs().Skip(1).ToList();
            if (args.Count == 0)
                return;

            foreach (var arg in args)
            {
                if (arg.StartsWith("-"))
                {
                    var parts = arg.Split('=');

                    if (parts.Length == 2)
                    {
                        var name = parts[0].TrimStart('-');
                        var value = parts[1];
                        CommandLineArguments[name] = value;
                    }
                    else
                    {
                        Console.WriteLine("Invalid argument format: " + arg);
                    }
                }
            }
        }

        public static Dictionary<string, string> CommandLineArguments { get; } = new();
    }
}
