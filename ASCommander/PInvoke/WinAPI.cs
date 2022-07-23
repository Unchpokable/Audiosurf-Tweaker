using System;
using System.Runtime.InteropServices;

namespace ASCommander.PInvoke
{
    public static class WinAPI
    {
        public const uint WM_COPYDATA = 0x4A;
        public const uint WM_ACTIVATEAPP = 0x001C;

        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = false)]
        public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, ref COPYDATASTRUCT lParam);


        [DllImport("user32.dll", SetLastError = true)]
        public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

        [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        public static extern bool IsWindow(IntPtr hWnd);
    }
}
