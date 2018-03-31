// remote.cpp : works with remote memory
//

#include "stdafx.h"

//do not call this function with  suspended hProcess
//free() return value
HMODULE* GetRemoteModules(HANDLE hProcess, DWORD* pnModules)
{
	DWORD cbNeeded, cb = 1 * sizeof(HMODULE);
	HMODULE* phModules = nullptr;
	for (;;)
	{
		phModules = (HMODULE*)malloc(cb);
		BOOL res = EnumProcessModulesEx(
			hProcess,
			phModules,
			cb,
			&cbNeeded,
			LIST_MODULES_ALL);
		if (res == 0)
		{
			printf("EnumProcessModulesEx returns %d, cbNeeded is %d \n",
				GetLastError(), cbNeeded);
			free(phModules);
			return nullptr;
		}

		if (cb == cbNeeded)
		{
			printf("Success, cbNeeded is %d \n", cbNeeded);
			break;
		}

		free(phModules);
		cb = cbNeeded;
		continue;
	}

	*pnModules = cb / sizeof(HMODULE);
	return phModules;
}

//execute custom callback for each worker
void RemoteModuleWorker(HANDLE hProcess, HMODULE* phModules, DWORD nModules,
	ModuleCallback Worker, PVOID WorkerContext)
{
	for (int i = 0; i < nModules; i++)
	{
		ULONG_PTR module = (ULONG_PTR)(phModules[i]); \
			printf("module %d at %p \n", i, module);
		if (!Worker(hProcess, module, WorkerContext)) break;
	}
}

PIMAGE_NT_HEADERS GetLocalPeHeader(REMOTE_ARGS_DEFS, bool* pis64)
{
	PIMAGE_DOS_HEADER pLocalDosHeader = REMOTE(IMAGE_DOS_HEADER, pRemoteImageBase);
	ULONG_PTR pRemotePeHeader =
		RVA_TO_REMOTE_VA(PIMAGE_NT_HEADERS, pLocalDosHeader->e_lfanew);
	PIMAGE_NT_HEADERS pLocalPeHeader = REMOTE(IMAGE_NT_HEADERS, pRemotePeHeader);

#if 0
	//check PE signature
	DWORD Signature = pLocalPeHeader->Signature;
	BYTE sig0 = ((BYTE*)&Signature)[0];
	BYTE sig1 = ((BYTE*)&Signature)[1];
	BYTE sig2 = ((BYTE*)&Signature)[2];
	BYTE sig3 = ((BYTE*)&Signature)[3];
	printf("PeHeader starts with %c%c%d%d \n", sig0, sig1, sig2, sig3);

	BYTE* sig = REMOTE_ARRAY_FIXED(BYTE, pRemotePeHeader, 4);
	printf("PeHeader starts with %c%c%d%d \n", sig[0], sig[1], sig[2], sig[3]);

	DWORD n;
	BYTE* sig42 = REMOTE_ARRAY_ZEROENDED(BYTE, pRemotePeHeader, n);
	printf("PeHeader starts with %s (strlen=%d) \n", sig42, n);
#endif

	switch (pLocalPeHeader->FileHeader.Machine)
	{
	case IMAGE_FILE_MACHINE_I386:
		assert(sizeof(IMAGE_OPTIONAL_HEADER32) ==
			pLocalPeHeader->FileHeader.SizeOfOptionalHeader);
		{
			*pis64 = false;
			PIMAGE_NT_HEADERS32 pLocalPeHeader2 = REMOTE(IMAGE_NT_HEADERS32, pRemotePeHeader);
			free(pLocalPeHeader);
			return (PIMAGE_NT_HEADERS)pLocalPeHeader2;
		}
	case IMAGE_FILE_MACHINE_AMD64:
		assert(sizeof(IMAGE_OPTIONAL_HEADER64) ==
			pLocalPeHeader->FileHeader.SizeOfOptionalHeader);
		{
			*pis64 = true;
			PIMAGE_NT_HEADERS64 pLocalPeHeader2 = REMOTE(IMAGE_NT_HEADERS64, pRemotePeHeader);
			free(pLocalPeHeader);
			return (PIMAGE_NT_HEADERS)pLocalPeHeader2;
		}
	default:
		printf("Unsupported hardware platfrom, Machine is %d",
			pLocalPeHeader->FileHeader.Machine);
		free(pLocalPeHeader);
		return nullptr;
	}
}

//some artifact about working with imports
#if 0
if (is64)
{
	PIMAGE_NT_HEADERS64 pLocalPeHeader3 = (PIMAGE_NT_HEADERS64)pLocalPeHeader2;

	ULONG_PTR pRemoteImageImportDescriptor = RVA_TO_REMOTE_VA(
		PIMAGE_IMPORT_DESCRIPTOR,
		pLocalPeHeader3->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	DWORD nDescs = 0;
	PIMAGE_IMPORT_DESCRIPTOR pLocalImageImportDescriptor =
		REMOTE_ARRAY_ZEROENDED(IMAGE_IMPORT_DESCRIPTOR, pRemoteImageImportDescriptor, nDescs);

	for (DWORD i = 0; i < nDescs; i++)
	{
		ULONG_PTR pRemoteDllName = RVA_TO_REMOTE_VA(
			char*,
			pLocalImageImportDescriptor[i].Name);

		char* pLocalDllName = REMOTE_ARRAY_ZEROENDED_NOLEN(char, pRemoteDllName);
		printf("Imports from %s \n", pLocalDllName);
	}
}
#endif