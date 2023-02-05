using System;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using ASCommander.DllInject.Exceptions;

namespace ASCommander.DllInject
{
    using static InjectNatives;

    public class DllInjector
    {
        public bool Inject(string dllName, string procName, out Exception thrownException)
        {
            try
            {
                var targetProc = Process.GetProcessesByName(procName).FirstOrDefault();
                if (targetProc == null)
                {
                    thrownException = new ProcessNotFoundException($"Couldn't find '{procName}'");
                    return false;
                }

                var procHandle = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, false, targetProc.Id);
                if (procHandle == IntPtr.Zero)
                {
                    thrownException = new ProcessAcessDeniedException($"Could not open process {procName}");
                    return false;
                }

                var kernel32dllHandle = GetModuleHandle("kernel32.dll");
                if (kernel32dllHandle == IntPtr.Zero)
                {
                    thrownException = new GenericInjectionException("Could not get handle of kernel32.dll");
                    return false;
                }

                var loadLibPtr = GetProcAddress(kernel32dllHandle, "LoadLibraryA");
                if (loadLibPtr == IntPtr.Zero)
                {
                    thrownException = new GenericInjectionException("Could not handle 'LoadLibraryA' function of kernel32.dll");
                    return false;
                }

                var allocMemPtr = VirtualAllocEx(procHandle, IntPtr.Zero, (uint)((dllName.Length + 1) * Marshal.SizeOf(typeof(char))), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (allocMemPtr == IntPtr.Zero)
                {
                    thrownException = new MemoryAllocationException($"Error during allocation memory in {procHandle} for {dllName} inject");
                    return false;
                }

                if (!WriteProcessMemory(procHandle, allocMemPtr, Encoding.Default.GetBytes(dllName), (uint)((dllName.Length + 1) * Marshal.SizeOf(typeof(char))), out UIntPtr bytesWritten))
                {
                    thrownException = new GenericInjectionException("Process memory write failed");
                    return false;
                }

                var attachedDllPtr = CreateRemoteThread(procHandle, IntPtr.Zero, 0, loadLibPtr, allocMemPtr, 0, IntPtr.Zero);

                if (attachedDllPtr == IntPtr.Zero)
                {
                    thrownException = new GenericInjectionException("Create remote thread returned nullptr. Dll injection failed");
                    return false;
                }

                thrownException = null;
                return true;
            }

            catch (Exception e)
            {
                thrownException = e;
                return false;
            }
        }
    }
}
