using SkinChangerRestyle.Core;

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
                _definedName = value;
                OnPropertyChanged(nameof(DefinedName));
            }
        }

        public string NameValue
        {
            get => _nameValue;
            set
            {
                _nameValue = value;
                OnPropertyChanged(nameof(NameValue));
            }
        }

        private string _definedName;
        private string _nameValue;

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
