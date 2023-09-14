namespace SkinChangerRestyle.MVVM.Model
{
    internal class GuidePageHelper
    {
        private static string _serverSwapperGuide = "Docs\\ServerSwapper.html";
        private static string _overlayGide = "Docs\\OverlayTroubleshooting.html";

        public static void ShowServerSwapperGuile()
        {
            new GuidancePage(_serverSwapperGuide).Show();
        }

        public static void ShowOverlayHelp()
        {
            new GuidancePage(_overlayGide).Show();
        }
    }
}
