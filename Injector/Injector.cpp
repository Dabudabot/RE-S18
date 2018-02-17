// Injector.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

BOOL InjectDll(HANDLE hProcess, LPCTSTR lpDllName)
{
	DWORD tid;
	
	DWORD lpDllName_size = (wcslen(lpDllName) + 1) * sizeof(wchar_t);
	PVOID lpDllName_remote = VirtualAllocEx(hProcess, 
		NULL, lpDllName_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	
	if (NULL == lpDllName_remote) 
	{
		printf("VirtualAllocEx returns NULL \n");
	}

	SIZE_T bytesWritten;
	BOOL bWriteProcessMemory_result = WriteProcessMemory(hProcess, 
		lpDllName_remote, lpDllName, lpDllName_size, &bytesWritten);

	if (FALSE == bWriteProcessMemory_result) 
	{

	}


	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, 
		(LPTHREAD_START_ROUTINE)LoadLibrary, lpDllName_remote, 0, &tid);
	if (hThread == NULL)
	{
		DWORD err = GetLastError();
		printf("Create thread failed with error: &d \n", err);
	}
}


int main()
{
#if 0
	DWORD tid;
	LPCTSTR cstr = L"MyDll.dll";
	DWORD cstr_sz = (wcslen(cstr) + 1) * sizeof(wchar_t);
	PVOID dllPath = VirtualAlloc(NULL, cstr_sz, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	memcpy(dllPath, cstr, cstr_sz);

	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibrary, dllPath, 0, &tid);
	if (hThread == NULL) 
	{
		DWORD err = GetLastError();
		printf("Create thread failed with error: &d \n", err);
	}
#endif


	//STARTUPINFO si;
	//PROCESS_INFORMATION pi;

	//ZeroMemory(&si, sizeof(si));	
	//si.cb = sizeof(si);
	//ZeroMemory(&pi, sizeof(pi));

	//if (!CreateProcess(
	//		TEXT("C:\\Windows\\System32\\calc.exe"),	//lpApplicationName
	//		NULL,										//lpCommandLine
	//		NULL,										//lpProcessAttributes
	//		NULL,										//lpThreadAttributes
	//		FALSE,										//bInheritHandles
	//		NORMAL_PRIORITY_CLASS,						//dwCreationFlags
	//		NULL,										//lpEnvironment
	//		NULL,										//lpCurrentDirectory
	//		&si,										//lpStartupInfo
	//		&pi))										//lpProcessInformation
	//{
	//	MessageBox(NULL, L"NOT CREATED", L"Injector.exe", MB_OK);
	//}
	//else 
	//{
	//	MessageBox(NULL, L"CREATED", L"Injector.exe", MB_OK);
	//}

	//LoadLibrary(L"MyDll.dll");

	system("pause");
    return 0;
}

