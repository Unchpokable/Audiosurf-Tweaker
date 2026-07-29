using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AudiosurfInterface
{
    public class ASHandleState
    {
        public string Message { get; private set; }
        public string ColorInterpretation { get; private set; }

        private static string _asNotConnectedStatusColor = "#ff0000";
        private static string _asConnectedStatusColor = "#11ff00";
        private static string _asWaitForRegistratingColor = "#ffff00";
        private static string _asCommunicationBrokenColor = "#ff8800";
        private static string _asSuspendedColor = "#888888";

        private static ASHandleState _connectedState;
        private static ASHandleState _authorizationAwaitingState;
        private static ASHandleState _notConnectedState;
        private static ASHandleState _communicationBrokenState;
        private static ASHandleState _suspendedState;

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

        // Terminal state: the game never acknowledged registration through the quickstart attempt,
        // the plain-command retry, or a bridge restart. Only cleared by a fresh registration cycle
        // (game window lost and found again) or a manual bridge reset - see AudiosurfHandle's
        // registration watchdog.
        public static ASHandleState CommunicationBroken
        {
            get
            {
                if (_communicationBrokenState != null) return _communicationBrokenState;
                _communicationBrokenState = new ASHandleState(
                    "Audiosurf did not respond - restart the tweaker, the game, or your PC", _asCommunicationBrokenColor);
                return _communicationBrokenState;
            }
        }

        // Deliberately stopped, not failed: the interface has torn its own bridge down because
        // something in the game process makes talking to it unsafe (a stale, unresponsive overlay
        // plugin - see AudiosurfHandle.SuspendService). Unlike every other state, nothing recovers
        // from this on its own; the user restarts the game and hits Reset.
        public static ASHandleState Suspended
        {
            get
            {
                if (_suspendedState != null) return _suspendedState;
                _suspendedState = new ASHandleState(
                    "Service stopped - restart Audiosurf, then press Reset", _asSuspendedColor);
                return _suspendedState;
            }
        }
    }
}
