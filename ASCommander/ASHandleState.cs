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

        private static string asNotConnectedStatusColor = "#ff0000";
        private static string asConnectedStatusColor = "#11ff00";
        private static string asWaitForRegistratingColor = "#ffff00";

        private static ASHandleState connectedState;
        private static ASHandleState authorizationAwaitingState;
        private static ASHandleState notConnectedState;

        private ASHandleState(string message, string hexColor)
        {
            Message = message;
            ColorInterpretation = hexColor;
        }

        public static ASHandleState Connected
        {
            get
            {
                if (connectedState != null) return connectedState;
                connectedState = new ASHandleState("Audiosurf connected", asConnectedStatusColor);
                return connectedState;
            }
        }

        public static ASHandleState Awaiting
        {
            get
            {
                if (authorizationAwaitingState != null) return authorizationAwaitingState;
                authorizationAwaitingState = new ASHandleState("Handled. Wait for AS approve", asWaitForRegistratingColor);
                return authorizationAwaitingState;
            }
        }

        public static ASHandleState NotConnected
        {
            get
            {
                if (notConnectedState != null) return notConnectedState;
                notConnectedState = new ASHandleState("Audiosurf not connected", asNotConnectedStatusColor);
                return notConnectedState;
            }
        }
    }
}
