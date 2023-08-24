using SkinChangerRestyle.Core;
using System;

namespace SkinChangerRestyle.MVVM.Model
{
    public class InterpreterVariable : ObservableObject
    {
        public InterpreterVariable() { }

        public InterpreterVariable(string definedName, string nameValue)
        {
            _definedName = definedName;
            _nameValue = nameValue;
        }

        public InterpreterVariable(InterpreterVariable origin)
        {
            _definedName = origin.Name;
            _nameValue = origin.Value;
        }

        public string Name
        {
            get => _definedName;
            set
            {
                if (!_definedNameEditable)
                    throw new InvalidOperationException("Resctricted operation");
                _definedName = value;
                OnPropertyChanged();
            }
        }

        public string Value
        {
            get => _nameValue;
            set
            {
                if (!_nameValueEditable)
                        throw new InvalidOperationException("Resctricted Operation"); 

                _nameValue = value;
                OnPropertyChanged();
            }
        }

        public bool NameEditable
        {
            get => _definedNameEditable;
            set
            {
                _definedNameEditable = value;
                OnPropertyChanged();
            }
        }

        public bool Freezed
        {
            get => !_definedNameEditable && !_nameValueEditable;
            set
            {
                NameEditable = !value;
                ValueEditable = !value;
                OnPropertyChanged();
            }
        }

        public bool ValueEditable
        {
            get => _nameValueEditable;
            set
            {
                _nameValueEditable = value;
                OnPropertyChanged();
            }
        }

        private string _definedName;
        private string _nameValue;

        private bool _definedNameEditable = true;
        private bool _nameValueEditable = true;

        public InterpreterVariable Clone()
        {
            return new InterpreterVariable(this);
        }

        public override string ToString()
        {
            return $"{_definedName} :: {_nameValue}";
        }
    }
}
