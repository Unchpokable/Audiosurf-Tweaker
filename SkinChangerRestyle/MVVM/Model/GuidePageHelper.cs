namespace SkinChangerRestyle.MVVM.Model
{
    internal class GuidePageHelper
    {
        private static string _serverSwapperGuide = "Docs\\ServerSwapper.html";

        public static void ShowServerSwapperGuile()
        {
            new GuidancePage(_serverSwapperGuide).Show();
        }
    }
}
