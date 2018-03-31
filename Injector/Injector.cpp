// Injector.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define SIC_MARK

HMODULE WINAPI MyLoadLibrary(
	_In_ LPCTSTR lpFileName
)
{
	return ::LoadLibrary(lpFileName);
}

BOOL InjectDll(HANDLE hProcess, LPCTSTR lpFileName, PVOID pfnLoadLibrary)
{
	BOOL ret = FALSE;
	PVOID lpFileName_remote = NULL;
	HANDLE hRemoteThread = NULL;

	for (;;)
	{
		//allocate remote storage
		DWORD lpFileName_size = (wcslen(lpFileName) + 1) * sizeof(wchar_t);
		lpFileName_remote = VirtualAllocEx(hProcess, NULL,
			lpFileName_size, MEM_COMMIT, PAGE_READWRITE);
		if (NULL == lpFileName_remote)
		{
			printf("VirtualAllocEx returns NULL \n");
			break;
		}

		//fill remote storage with data
		SIZE_T bytesWritten;
		BOOL res = WriteProcessMemory(hProcess, lpFileName_remote,
			lpFileName, lpFileName_size, &bytesWritten);
		if (FALSE == res)
		{
			printf("WriteProcessMemory failed with %d \n", GetLastError());
			break;
		}

		DWORD tid;
		//in case of problems try MyLoadLibrary if this is actually current process
		hRemoteThread = CreateRemoteThread(hProcess,
			NULL, 0, (LPTHREAD_START_ROUTINE)pfnLoadLibrary, lpFileName_remote,
			0, &tid);
		if (NULL == hRemoteThread)
		{
			printf("CreateRemoteThread failed with %d \n", GetLastError());
			break;
		}

		//wait for MyDll initialization
		WaitForSingleObject(hRemoteThread, INFINITE);
		//FUTURE obtain exit code and check

		ret = TRUE;
		break;
	}

	if (!ret)
	{
		if (lpFileName_remote) SIC_MARK;
		//TODO call VirtualFree(...)
	}

	if (hRemoteThread) CloseHandle(hRemoteThread);
	return ret;
}

//returns entry point in remote process
ULONG_PTR GetEnryPoint(REMOTE_ARGS_DEFS)
{
	return 0;
}

//returns address of LoadLibraryA in remote process
ULONG_PTR GetRemoteLoadLibraryA(REMOTE_ARGS_DEFS)
{
	return 0;
}

typedef struct _EXPORT_CONTEXT
{
	char* ModuleName;
	char* FunctionName;
	ULONG_PTR RemoteFunctionAddress;
} EXPORT_CONTEXT, *PEXPORT_CONTEXT;

bool FindExport(REMOTE_ARGS_DEFS, PVOID Context)
{
	PEXPORT_CONTEXT ExportContext = (PEXPORT_CONTEXT)Context;
	ExportContext->RemoteFunctionAddress = 0;
	bool bFound = false; //have I found export in this module?
	bool bIterateMore = true; //should we iterate next module?

	bool is64;
	PIMAGE_NT_HEADERS pLocalPeHeader = GetLocalPeHeader(REMOTE_ARGS_CALL, &is64);
	ULONG_PTR pRemoteImageExportDescriptor;

	if (is64)
	{
		PIMAGE_NT_HEADERS64 pLocalPeHeader2 = (PIMAGE_NT_HEADERS64)pLocalPeHeader;
		pRemoteImageExportDescriptor = RVA_TO_REMOTE_VA(
			PIMAGE_EXPORT_DIRECTORY,
			pLocalPeHeader2->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	}
	else
	{
		PIMAGE_NT_HEADERS32 pLocalPeHeader2 = (PIMAGE_NT_HEADERS32)pLocalPeHeader;
		pRemoteImageExportDescriptor = RVA_TO_REMOTE_VA(
			PIMAGE_EXPORT_DIRECTORY,
			pLocalPeHeader2->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	}
	free(pLocalPeHeader);

	PIMAGE_EXPORT_DIRECTORY pLocalImageExportDescriptor =
		REMOTE(IMAGE_EXPORT_DIRECTORY, pRemoteImageExportDescriptor);

	char* pLocalDllName = nullptr;
	DWORD* pLocalNames = nullptr;
	DWORD* pLocalAddrs = nullptr;

	for (;;)
	{
		if (!pLocalImageExportDescriptor) break; //no export table, iterate next module

		ULONG_PTR pRemoteDllName = RVA_TO_REMOTE_VA(
			char*,
			pLocalImageExportDescriptor->Name);
		pLocalDllName = REMOTE_ARRAY_ZEROENDED_NOLEN(char, pRemoteDllName);

		printf("dllName is %s \n", pLocalDllName);

		if (0 != strcmp(ExportContext->ModuleName, pLocalDllName)) break; //not our dll, iterate next module
		bIterateMore = false; //we've found our dll no need to iterate more modules

		ULONG_PTR pRemoteNames =
			RVA_TO_REMOTE_VA(DWORD*, pLocalImageExportDescriptor->AddressOfNames);
		ULONG_PTR pRemoteAddrs =
			RVA_TO_REMOTE_VA(DWORD*, pLocalImageExportDescriptor->AddressOfFunctions);

		pLocalNames = REMOTE_ARRAY_FIXED(DWORD, pRemoteNames, pLocalImageExportDescriptor->NumberOfNames);
		pLocalAddrs = REMOTE_ARRAY_FIXED(DWORD, pRemoteAddrs, pLocalImageExportDescriptor->NumberOfFunctions);

		if (pLocalImageExportDescriptor->NumberOfNames != pLocalImageExportDescriptor->NumberOfFunctions)
		{
			printf("FindExport: ERROR: VERY STRANGE mismatch NumberOfNames vs NumberOfFunctions \n");
			//TODO printf args ...
			break;
		}

		for (int i = 0; i < pLocalImageExportDescriptor->NumberOfNames; i++)
		{
			ULONG_PTR pRemoteString = RVA_TO_REMOTE_VA(char*, pLocalNames[i]);
			char* pLocalString = REMOTE_ARRAY_ZEROENDED_NOLEN(char, pRemoteString);

			//printf("Function: %s at %p \n", pLocalString, pLocalAddrs[i]);

			if (0 == strcmp(ExportContext->FunctionName, pLocalString))
			{
				bFound = true; //stop iterating, we found it
				ExportContext->RemoteFunctionAddress = pRemoteImageBase + pLocalAddrs[i];
				free(pLocalString);
				break;
			}
			free(pLocalString);
		}

		break;
	}

	if ((!bIterateMore)&(!bFound))
	{
		printf("FindExport: ERROR: VERY STRANGE function was not found \n");
	}

	if (pLocalNames) free(pLocalNames);
	if (pLocalAddrs) free(pLocalAddrs);
	if (pLocalImageExportDescriptor) free(pLocalImageExportDescriptor);
	if (pLocalDllName) free(pLocalDllName);
	return bIterateMore;
}

int _tmain(int argc, _TCHAR* argv[])
{
	STARTUPINFO info = { sizeof(info) };
	PROCESS_INFORMATION processInfo;
	::CreateProcess(L"C:\\Windows\\system32\\notepad.exe", NULL,
		NULL, NULL, TRUE, 0 /*CREATE_SUSPENDED*/, NULL, NULL, &info, &processInfo);

	Sleep(1000);

	DWORD nModules;
	HMODULE* phModules = GetRemoteModules(processInfo.hProcess, &nModules);
	EXPORT_CONTEXT MyContext;
	MyContext.ModuleName = "KERNEL32.dll";
	MyContext.FunctionName = "LoadLibraryA";
	MyContext.RemoteFunctionAddress = 0;
	RemoteModuleWorker(processInfo.hProcess, phModules, nModules, FindExport, &MyContext);
	printf("kernel32!LoadLibraryA is at %p \n", MyContext.RemoteFunctionAddress);

#if 0
	//magic to calculate remote LoadLibraryW
	//if you'll manually adjust pKernel32base_remote via Debugger
	HMODULE pKernel32base = GetModuleHandle(L"kernel32.dll");
	PVOID pLoadLibraryW = GetProcAddress(pKernel32base, "LoadLibraryW");
	DWORD_PTR ofs_LoadLibrary =
		((DWORD_PTR)pLoadLibraryW) - ((DWORD_PTR)pKernel32base);
	HMODULE pKernel32base_remote = NULL;
	PVOID pLoadLibraryW_remote =
		(PVOID)((DWORD_PTR(pKernel32base_remote)) + ofs_LoadLibrary);
#endif

#if 0
	//HANDLE hProcess = GetCurrentProcess();
	HANDLE hProcess = processInfo.hProcess;
	InjectDll(hProcess, L"D:\\PROJECTS\\Inno-S18-RevEng-DllInjection\\Debug\\MyDll.dll",
#if 0
		pLoadLibraryW_remote);
#else
		LoadLibrary);
#endif
	Sleep(10000);
#endif

#if 0
	LPCTSTR cstr = L"MyDll.dll";
	DWORD cstr_sz = (wcslen(cstr) + 1) * sizeof(wchar_t);
	DWORD tid;

	PVOID pstr = VirtualAlloc(NULL, cstr_sz, MEM_COMMIT, PAGE_READWRITE);
	memcpy(pstr, cstr, cstr_sz);
	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibrary, pstr, 0, &tid);
	if (hThread == NULL)
	{
		DWORD err = GetLastError();
		printf("CreateThread failed with %d \n", err);
	}
#endif 

#if 0
	::LoadLibrary(L"MyDll.dll");
#endif

	/*
	WaitForSingleObject(processInfo.hProcess, INFINITE);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);
	*/
	return 0;
}