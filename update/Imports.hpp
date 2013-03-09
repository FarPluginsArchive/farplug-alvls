#pragma once

#include "headers.hpp"

typedef BOOL (WINAPI *ATTACHCONSOLE)(DWORD dwProcessId);
typedef BOOL (WINAPI *ISWOW64PROCESS)(HANDLE hProcess,PBOOL Wow64Process);
typedef LONG (WINAPI *NTQUERYINFORMATIONPROCESS)(HANDLE ProcessHandle,PROCESSINFOCLASS ProcessInformationClass,PVOID ProcessInformation,ULONG ProcessInformationLength,PULONG ReturnLength);
typedef BOOL (WINAPI *QUERYFULLPROCESSIMAGENAMEW)(HANDLE Process,DWORD Flags,LPWSTR ExeName,PDWORD Size);


struct ImportedFunctions
{
private:
	ATTACHCONSOLE pAttachConsole;
	ISWOW64PROCESS pIsWow64Process;
	NTQUERYINFORMATIONPROCESS pNtQueryInformationProcess;
	QUERYFULLPROCESSIMAGENAMEW pQueryFullProcessImageNameW;

public:
	VOID Load();
	BOOL AttachConsole(DWORD dwProcessId);
	BOOL IsWow64Process(HANDLE hProcess,PBOOL Wow64Process);
	LONG NtQueryInformationProcess(HANDLE ProcessHandle,PROCESSINFOCLASS ProcessInformationClass,PVOID ProcessInformation,ULONG ProcessInformationLength,PULONG ReturnLength);
	BOOL QueryFullProcessImageNameW(HANDLE Process,DWORD Flags,LPWSTR ExeName,PDWORD Size);
};

extern ImportedFunctions ifn;
