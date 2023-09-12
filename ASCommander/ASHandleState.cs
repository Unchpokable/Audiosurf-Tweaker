using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ASCommander
{
    public class ASHandleState
    {
        public string Message { get; private set; }
        public string ColorInterpretation { get; private set; }

        private static string _asNotConnectedStatusColor = "#ff0000";
        private static string _asConnectedStatusColor = "#11ff00";
        private static string _asWaitForRegistratingColor = "#ffff00";

        private static ASHandleState _connectedState;
        private static ASHandleState _authorizationAwaitingState;
        private static ASHandleState _notConnectedState;

        private ASHandleState(string message, string hexColor)
        {
            Message = message;
            ColorInterpretation = hexColor;
        }

        public static ASHandleState Connected
        {
            get
            {
                if (_connectedState != null) return _connectedState;
                _connectedState = new ASHandleState("Audiosurf connected", _asConnectedStatusColor);
                return _connectedState;
            }
        }

        public static ASHandleState Awaiting
        {
            get
            {
                if (_authorizationAwaitingState != null) return _authorizationAwaitingState;
                _authorizationAwaitingState = new ASHandleState("Handled. Wait for AS approve", _asWaitForRegistratingColor);
                return _authorizationAwaitingState;
            }
        }

        public static ASHandleState NotConnected
        {
            get
            {
                if (_notConnectedState != null) return _notConnectedState;
                _notConnectedState = new ASHandleState("Audiosurf not connected", _asNotConnectedStatusColor);
                return _notConnectedState;
            }
        }
    }
}
