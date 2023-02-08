#include <iostream>
#include <Windows.h>
#include <filesystem>
#include <fstream>

constexpr auto REQUIERED_COMMANDLINE_ARGUMENT_COUNT = 3;

BOOL Inject(DWORD, LPSTR);

int main(int argc, char** argv)
{
	if (argc != REQUIERED_COMMANDLINE_ARGUMENT_COUNT)
		return 0;

	auto procID = std::atoi(argv[1]);

	LPSTR dllPath(argv[2]);
	
	std::ofstream out("output.txt");
	std::cout.rdbuf(out.rdbuf());

	std::cout << "[Call args] pID: " << procID << "\nDll Path: " << dllPath << "\n";

	if (!Inject(procID, dllPath)) 
	{
		std::cout << "Injection failed\n";
	}
	else 
		std::cout << "Injection success\n";
	return 0;
}

BOOL Inject(DWORD pID, LPSTR dllPath)
{
	HANDLE procHandle = OpenProcess(PROCESS_ALL_ACCESS, false, pID);

	if (procHandle)
	{
		LPVOID loadLibAddr = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
		LPVOID targetProcAllocMem = VirtualAllocEx(procHandle, NULL, strlen(dllPath), 
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

		if (targetProcAllocMem == nullptr)
			return FALSE;

		WriteProcessMemory(procHandle, targetProcAllocMem, dllPath, strlen(dllPath), NULL);

		HANDLE hRemoteThread = CreateRemoteThread(procHandle, NULL, NULL, 
			(LPTHREAD_START_ROUTINE)loadLibAddr, targetProcAllocMem, 0, NULL);

		if (hRemoteThread == nullptr)
			return FALSE;

		WaitForSingleObject(hRemoteThread, INFINITE);
		VirtualFreeEx(procHandle, targetProcAllocMem, strlen(dllPath), MEM_RELEASE);
		CloseHandle(hRemoteThread);
		CloseHandle(procHandle);
		return TRUE;
	}

	return FALSE;
}