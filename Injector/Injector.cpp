// Injector.cpp : Defines the entry point for the console application.
//
// Funtions googled:
// LoadLibrary - to load your dll (eg mydll.dll)
// CreateProcess - to create some process (eg notepad.exe)

#include "stdafx.h"

int _tmain(int argc, _TCHAR* argv[])
{
	LoadLibrary(L"MyDll.dll");

	//notepad is created
#if 0
	STARTUPINFO info = { sizeof(info) };
	PROCESS_INFORMATION process_info;
	CreateProcess(L"C:\\Windows\\system32\\notepad.exe", 
		nullptr, nullptr, nullptr, 
		true, 0, nullptr, 
		nullptr, &info, &process_info);
#endif

	/*
	WaitForSingleObject(processInfo.hProcess, INFINITE);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);
	*/

	return 0;
}