#include <iostream>
#include <Windows.h>

constexpr auto REQUIERED_COMMANDLINE_ARGUMENT_COUNT = 3;

BOOL Inject(DWORD, LPSTR);

int main(int argc, char** argv)
{
	if (argc != REQUIERED_COMMANDLINE_ARGUMENT_COUNT)
	{
		std::cerr << "E_ARGC usage: InjectHelper.exe <pid> <dllPath>" << std::endl;
		return -1;
	}

	auto procID = std::atoi(argv[1]);

	LPSTR dllPath(argv[2]);


	if (!Inject(procID, dllPath))
	{
		return -1;
	}
	return 0;
}

BOOL Inject(DWORD pID, LPSTR dllPath)
{
	HANDLE procHandle = OpenProcess(PROCESS_ALL_ACCESS, false, pID);

	if (!procHandle)
	{
		std::cerr << "E_OPEN_PROCESS GetLastError=" << GetLastError() << std::endl;
		return FALSE;
	}

	LPVOID loadLibAddr = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
	// +1: the remote LoadLibraryA call needs the null terminator too, not just the path bytes.
	SIZE_T pathSize = strlen(dllPath) + 1;
	LPVOID targetProcAllocMem = VirtualAllocEx(procHandle, NULL, pathSize,
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	if (targetProcAllocMem == nullptr)
	{
		std::cerr << "E_ALLOC GetLastError=" << GetLastError() << std::endl;
		CloseHandle(procHandle);
		return FALSE;
	}

	if (!WriteProcessMemory(procHandle, targetProcAllocMem, dllPath, pathSize, NULL))
	{
		std::cerr << "E_WRITE_MEMORY GetLastError=" << GetLastError() << std::endl;
		VirtualFreeEx(procHandle, targetProcAllocMem, 0, MEM_RELEASE);
		CloseHandle(procHandle);
		return FALSE;
	}

	HANDLE hRemoteThread = CreateRemoteThread(procHandle, NULL, NULL,
		(LPTHREAD_START_ROUTINE)loadLibAddr, targetProcAllocMem, 0, NULL);

	if (hRemoteThread == nullptr)
	{
		std::cerr << "E_REMOTE_THREAD GetLastError=" << GetLastError() << std::endl;
		VirtualFreeEx(procHandle, targetProcAllocMem, 0, MEM_RELEASE);
		CloseHandle(procHandle);
		return FALSE;
	}

	WaitForSingleObject(hRemoteThread, INFINITE);
	VirtualFreeEx(procHandle, targetProcAllocMem, 0, MEM_RELEASE);
	CloseHandle(hRemoteThread);
	CloseHandle(procHandle);
	return TRUE;
}