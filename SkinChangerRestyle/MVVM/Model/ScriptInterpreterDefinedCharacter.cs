using SkinChangerRestyle.Core;
using System;

namespace SkinChangerRestyle.MVVM.Model
{
    public class ScriptInterpreterDefinedCharacter : ObservableObject
    {
        public ScriptInterpreterDefinedCharacter() { }

        public ScriptInterpreterDefinedCharacter(string definedName, string nameValue)
        {
            _definedName = definedName;
            _nameValue = nameValue;
        }

        public ScriptInterpreterDefinedCharacter(ScriptInterpreterDefinedCharacter origin)
        {
            _definedName = origin.DefinedName;
            _nameValue = origin.NameValue;
        }

        public string DefinedName
        {
            get => _definedName;
            set
            {
                if (!_definedNameEditable)
                    throw new InvalidOperationException("Resctricted operation");
                _definedName = value;
                OnPropertyChanged(nameof(DefinedName));
            }
        }

        public string NameValue
        {
            get => _nameValue;
            set
            {
                if (!_nameValueEditable) 
                        throw new InvalidOperationException("Resctricted Operation"); 

                _nameValue = value;
                OnPropertyChanged(nameof(NameValue));
            }
        }

        public bool NameEditable
        {
            get => _definedNameEditable;
            set
            {
                _definedNameEditable = value;
                OnPropertyChanged(nameof(NameEditable));
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
                OnPropertyChanged(nameof(ValueEditable));
            }
        }

        private string _definedName;
        private string _nameValue;

        private bool _definedNameEditable = true;
        private bool _nameValueEditable = true;

        public ScriptInterpreterDefinedCharacter Clone()
        {
            return new ScriptInterpreterDefinedCharacter(this);
        }

        public override string ToString()
        {
            return $"{_definedName} :: {_nameValue}";
        }
    }
}
