using SkinChangerRestyle.Core;
using System;

namespace SkinChangerRestyle.MVVM.Model
{
    public class InterpreterVariable : ObservableObject, IEquatable<InterpreterVariable>
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

        public bool Equals(InterpreterVariable other)
        {
            if (ReferenceEquals(null, other)) return false;
            if (ReferenceEquals(this, other)) return true;
            return _definedName == other._definedName && _nameValue == other._nameValue && _definedNameEditable == other._definedNameEditable && _nameValueEditable == other._nameValueEditable;
        }

        public override bool Equals(object obj)
        {
            if (ReferenceEquals(null, obj)) return false;
            if (ReferenceEquals(this, obj)) return true;
            if (obj.GetType() != this.GetType()) return false;
            return Equals((InterpreterVariable)obj);
        }

        public override int GetHashCode()
        {
            unchecked
            {
                var hashCode = (_definedName != null ? _definedName.GetHashCode() : 0);
                hashCode = (hashCode * 397) ^ (_nameValue != null ? _nameValue.GetHashCode() : 0);
                hashCode = (hashCode * 397) ^ _definedNameEditable.GetHashCode();
                hashCode = (hashCode * 397) ^ _nameValueEditable.GetHashCode();
                return hashCode;
            }
        }
    }
}
