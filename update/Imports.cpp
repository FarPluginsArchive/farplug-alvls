#include "Headers.hpp"

#include "Imports.hpp"

ImportedFunctions ifn;

VOID ImportedFunctions::Load()
{
	HMODULE hKernel32=GetModuleHandle(L"kernel32.dll");
	if(hKernel32)
	{
		pAttachConsole=reinterpret_cast<ATTACHCONSOLE>(GetProcAddress(hKernel32,"AttachConsole"));
		pIsWow64Process=reinterpret_cast<ISWOW64PROCESS>(GetProcAddress(hKernel32,"IsWow64Process"));
		pQueryFullProcessImageNameW=reinterpret_cast<QUERYFULLPROCESSIMAGENAMEW>(GetProcAddress(hKernel32,"QueryFullProcessImageNameW"));
	}
	HMODULE hNtDll=GetModuleHandle(L"ntdll.dll");
	if(hNtDll)
	{
		pNtQueryInformationProcess=reinterpret_cast<NTQUERYINFORMATIONPROCESS>(GetProcAddress(hNtDll,"NtQueryInformationProcess"));
	}
}

BOOL ImportedFunctions::AttachConsole(DWORD dwProcessId)
{
	BOOL Ret=FALSE;
	if(pAttachConsole)
	{
		Ret=pAttachConsole(dwProcessId);
	}
	else
	{
		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	}
	return Ret;
}

BOOL ImportedFunctions::IsWow64Process(HANDLE hProcess,PBOOL Wow64Process)
{
	BOOL Ret=0;
	if(pIsWow64Process)
	{
		Ret=pIsWow64Process(hProcess,Wow64Process);
	}
	else
	{
		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	}
	return Ret;
}

LONG ImportedFunctions::NtQueryInformationProcess(HANDLE ProcessHandle,PROCESSINFOCLASS ProcessInformationClass,PVOID ProcessInformation,ULONG ProcessInformationLength,PULONG ReturnLength)
{
	LONG Ret=-1;
	if(pNtQueryInformationProcess)
	{
		Ret=pNtQueryInformationProcess(ProcessHandle,ProcessInformationClass,ProcessInformation,ProcessInformationLength,ReturnLength);
	}
	else
	{
		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	}
	return Ret;
}

BOOL ImportedFunctions::QueryFullProcessImageNameW(HANDLE Process, DWORD Flags, LPWSTR ExeName, PDWORD Size)
{
	if(pQueryFullProcessImageNameW)
	{
		return pQueryFullProcessImageNameW(Process, Flags, ExeName, Size);
	}
	else
	{
		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		return FALSE;
	}
}
