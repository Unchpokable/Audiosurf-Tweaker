namespace AudiosurfInterface
{
    public sealed class ASHandleState
    {
        public string Message { get; }
        public string ColorInterpretation { get; }

        private ASHandleState(string message, string hexColor)
        {
            Message = message;
            ColorInterpretation = hexColor;
        }

        public static readonly ASHandleState Connected = new ASHandleState("Audiosurf connected", "#11ff00");
        public static readonly ASHandleState Awaiting = new ASHandleState("Handled. Wait for AS approve", "#ffff00");
        public static readonly ASHandleState NotConnected = new ASHandleState("Audiosurf not connected", "#ff0000");

        // Terminal state: the game never acknowledged registration through the quickstart attempt,
        // the plain-command retry, or a bridge restart. Only cleared by a fresh registration cycle
        // (game window lost and found again) or a manual bridge reset - see AudiosurfHandle's
        // registration watchdog.
        public static readonly ASHandleState CommunicationBroken = new ASHandleState(
            "Audiosurf did not respond - restart the tweaker, the game, or your PC", "#ff8800");

        // Deliberately stopped, not failed: the interface has torn its own bridge down because
        // something in the game process makes talking to it unsafe (a stale, unresponsive overlay
        // plugin - see AudiosurfHandle.SuspendService). Unlike every other state, nothing recovers
        // from this on its own; the user restarts the game and hits Reset.
        public static readonly ASHandleState Suspended = new ASHandleState(
            "Service stopped - restart Audiosurf, then press Reset", "#888888");
    }
}
