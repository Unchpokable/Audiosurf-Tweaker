using System;
using System.Runtime.InteropServices;

namespace ASCommander.PInvoke
{

    [StructLayout(LayoutKind.Sequential)]
    public struct COPYDATASTRUCT
    {
        public IntPtr dwData;
        public int cbData;

        [MarshalAs(UnmanagedType.LPStr)]
        public string lpData;
    }
}
