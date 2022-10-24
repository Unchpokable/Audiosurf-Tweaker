using System;
using System.Windows.Input;


namespace SkinChangerRestyle.Core
{

    internal class RelayCommand : ICommand
    {
        protected Action<object> execute;
        protected Func<object, bool> canExecute;

        public event EventHandler CanExecuteChanged
        {
            add { CommandManager.RequerySuggested += value; }
            remove { CommandManager.RequerySuggested -= value; }
        }

        public RelayCommand(Action<object> exec, Func<object, bool> canExec = null)
        {
            execute = exec;
            canExecute = canExec;
        }

        public bool CanExecute(object parameter)
        {
            return canExecute == null || canExecute(parameter);
        }

        public void Execute(object parameter)
        {
            execute(parameter);
        }
    }
}
