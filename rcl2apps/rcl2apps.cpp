#include "headers.hpp"
#include "rcl2apps.hpp"

// {5AF8F265-49D1-4943-9BDE-68D5082C1D22}
DEFINE_GUID(MainGuid,0x5af8f265, 0x49d1, 0x4943, 0x9b, 0xde, 0x68, 0xd5, 0x8, 0x2c, 0x1d, 0x22);

PluginStartupInfo Info;
DWORD ControlKeyState;
BOOL Flag;
int Button;
HANDLE hInsuranceEvent;

typedef BOOL (WINAPI *READCONSOLEINPUT)(HANDLE, PINPUT_RECORD, DWORD, LPDWORD);
READCONSOLEINPUT Real_ReadConsoleInputW;

extern void WINAPI ProcessInput(INPUT_RECORD *InRec);

BOOL __stdcall Thunk_ReadConsoleInputW(HANDLE hConsoleInput, PINPUT_RECORD lpBuffer, DWORD nLength, LPDWORD lpNumberOfEventsRead)
{
	BOOL bResult = Real_ReadConsoleInputW(hConsoleInput, lpBuffer, nLength, lpNumberOfEventsRead);

	for (DWORD i = 0; i < *lpNumberOfEventsRead; i++)
		ProcessInput(&lpBuffer[i]);

	return bResult;
}

PROC RtlHookImportTable(const char *lpModuleName, const char *lpFunctionName, PROC pfnNew, HMODULE hModule)
{
	PBYTE pModule = (PBYTE)hModule;
	PROC pfnResult = NULL;
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pModule;

	if ( pDosHeader->e_magic != 'ZM' )
		return NULL;

	PIMAGE_NT_HEADERS pPEHeader  = (PIMAGE_NT_HEADERS)&pModule[pDosHeader->e_lfanew];

	if ( pPEHeader->Signature != 0x00004550 ) 
		return NULL;

	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)&pModule[pPEHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress];

	const char *lpImportTableFunctionName;

	if ( !pPEHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress )
		return NULL;

	const char *lpImportTableModuleName;

	while ( pImportDesc->Name )
	{
		lpImportTableModuleName = (const char*)&pModule[pImportDesc->Name];

		if ( !lstrcmpiA (lpImportTableModuleName, lpModuleName) ) 
			break;

		pImportDesc++;
	}

	if ( !pImportDesc->Name )
		return NULL;

	PIMAGE_THUNK_DATA pFirstThunk;
	PIMAGE_THUNK_DATA pOriginalThunk;
	
	pFirstThunk = (PIMAGE_THUNK_DATA)&pModule[pImportDesc->FirstThunk];	
	pOriginalThunk = (PIMAGE_THUNK_DATA)&pModule[pImportDesc->OriginalFirstThunk];	

	while ( pFirstThunk->u1.Function )
	{
		DWORD index = (DWORD)((PIMAGE_IMPORT_BY_NAME)pOriginalThunk->u1.AddressOfData)->Name;

		lpImportTableFunctionName = (const char*)&pModule[index];

		DWORD dwOldProtect;
		PROC* ppfnOld;

		if ( !lstrcmpiA (lpImportTableFunctionName, lpFunctionName) )
		{
			pfnResult = (PROC)pFirstThunk->u1.Function;
			ppfnOld = (PROC*)&pFirstThunk->u1.Function;

			VirtualProtect (ppfnOld, sizeof (PROC), PAGE_READWRITE, &dwOldProtect);
			WriteProcessMemory(GetCurrentProcess(), ppfnOld, &pfnNew, sizeof pfnNew, NULL);
			VirtualProtect (ppfnOld, sizeof (PROC), dwOldProtect, &dwOldProtect);

			return pfnResult;
		}
		
		pFirstThunk++;
		pOriginalThunk++;
	}

	return NULL; //error
}

__int64 GetFarSetting(FARSETTINGS_SUBFOLDERS Root,const wchar_t* Name)
{
	__int64 result=0;
	FarSettingsCreate settings={sizeof(FarSettingsCreate),FarGuid,INVALID_HANDLE_VALUE};
	HANDLE Settings=Info.SettingsControl(INVALID_HANDLE_VALUE,SCTL_CREATE,0,&settings)?settings.Handle:0;
	if (Settings)
	{
		FarSettingsItem item={sizeof(FarSettingsItem),Root,Name,FST_UNKNOWN,{0}};
		if(Info.SettingsControl(Settings,SCTL_GET,0,&item)&&FST_QWORD==item.Type)
		{
			result=item.Number;
		}
		Info.SettingsControl(Settings,SCTL_FREE,0,0);
	}
	return result;
}

bool CheckPanel(COORD Pos, PanelInfo* pInfo)
{
	int Top = 1+(GetFarSetting(FSSF_PANELLAYOUT,L"ColumnTitles")?1:0)+(GetFarSetting(FSSF_INTERFACE,L"ShowMenuBar")?1:0);
	int Bottom = 1+(GetFarSetting(FSSF_PANELLAYOUT,L"StatusLine")?2:0);

	if ( (Pos.X > pInfo->PanelRect.left) &&
		(Pos.X < pInfo->PanelRect.right) &&
		(Pos.Y >= pInfo->PanelRect.top+Top) &&
		(Pos.Y <= pInfo->PanelRect.bottom-Bottom) &&
		((pInfo->Flags&PFLAGS_VISIBLE)==PFLAGS_VISIBLE) )
	{
		if ( ((pInfo->Flags&PFLAGS_PLUGIN)!=PFLAGS_PLUGIN) || ((pInfo->Flags&PFLAGS_REALNAMES) == PFLAGS_REALNAMES) )
			return true;
	}
	return false;
}

bool IsPanel(COORD Pos)
{
	struct WindowType Type={sizeof(WindowType)};
	if (Info.AdvControl(&MainGuid,ACTL_GETWINDOWTYPE,0,&Type) && Type.Type==WTYPE_PANELS)
	{
		PanelInfo pnInfo={sizeof(PanelInfo)};
		PanelInfo pnOtherInfo={sizeof(PanelInfo)};

		if (Info.PanelControl(PANEL_ACTIVE,FCTL_GETPANELINFO,0,&pnInfo) && CheckPanel(Pos, &pnInfo))
			return true;

		if (Info.PanelControl(PANEL_PASSIVE,FCTL_GETPANELINFO,0,&pnOtherInfo) && CheckPanel(Pos, &pnOtherInfo))
			return true;
	}
	return false;
}

void WINAPI ProcessInput(INPUT_RECORD* InRec)
{
	DWORD dwReadCount;

	if ((InRec->EventType == MOUSE_EVENT) && !InRec->Event.MouseEvent.dwEventFlags)
	{
		if (InRec->Event.MouseEvent.dwButtonState == Button)
		{
			COORD pos = InRec->Event.MouseEvent.dwMousePosition;

			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

			pos.Y -= (csbi.dwSize.Y-(csbi.srWindow.Bottom-csbi.srWindow.Top+1));

			if (IsPanel(pos))
			{
				InRec->Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED;
				ControlKeyState = InRec->Event.MouseEvent.dwControlKeyState;
				Flag=1;
				WriteConsoleInput(GetStdHandle(STD_INPUT_HANDLE), InRec, 1, &dwReadCount);
			}
		}

		if ( Flag && !InRec->Event.MouseEvent.dwButtonState )
		{
			BYTE OutRec[]=
			{
				0x01,0x00,0x52,0xCA,0x01,0x00,0x00,0x00,0x01,0x00,
				0x5D,0x00,0x5D,0x00,0x00,0x00,0x20,0x01,0x00,0x00
			};

			((INPUT_RECORD *)OutRec)->Event.KeyEvent.dwControlKeyState=ControlKeyState;
			WriteConsoleInput(GetStdHandle(STD_INPUT_HANDLE), (INPUT_RECORD *)OutRec, 1, &dwReadCount);
			Flag = 0;
		}
	}
}

INT WINAPI GetMinFarVersionW()
{
#define MAKEFARVERSION(major,minor,build) ( ((major)<<8) | (minor) | ((build)<<16))
	return MAKEFARVERSION(MIN_FAR_MAJOR_VER,MIN_FAR_MINOR_VER,MIN_FAR_BUILD);
#undef MAKEFARVERSION
}

void WINAPI GetGlobalInfoW(GlobalInfo *Info)
{
	Info->StructSize=sizeof(GlobalInfo);
	Info->MinFarVersion=MAKEFARVERSION(MIN_FAR_MAJOR_VER,MIN_FAR_MINOR_VER,0,MIN_FAR_BUILD,VS_RELEASE);
	Info->Version=MAKEFARVERSION(MAJOR_VER,MINOR_VER,0,BUILD,VS_RELEASE);
	Info->Guid=MainGuid;
	Info->Title=L"rcl2apps";
	Info->Description=L"Right Click Menu Activator for Far Manager v.3.0";
	Info->Author=L"WARP_ItSelf, Alexey Samlyukov";
}

VOID WINAPI GetPluginInfoW(PluginInfo* pi)
{
	pi->StructSize = sizeof(PluginInfo);
	pi->Flags = PF_PRELOAD;
}

VOID WINAPI SetStartupInfoW(const PluginStartupInfo* pInfo)
{
	Info = *pInfo;
	wchar_t strIniName[MAX_PATH];
	lstrcpy(strIniName,pInfo->ModuleName);
	*(StrRChr(strIniName,NULL,L'\\')+1)=0;
	lstrcat(strIniName,L"rcl2apps.ini");
	Button = GetPrivateProfileInt(L"Options", L"Button", 2, strIniName);
	Real_ReadConsoleInputW = (READCONSOLEINPUT)RtlHookImportTable("kernel32.dll", "ReadConsoleInputW", (PROC)Thunk_ReadConsoleInputW, GetModuleHandle(NULL));
}

VOID WINAPI ExitFARW(ExitInfo* Info)
{
	RtlHookImportTable("kernel32.dll","ReadConsoleInputW",(PROC)Real_ReadConsoleInputW,GetModuleHandle(NULL));
}

BOOL __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		wchar_t strEventName[128];
		wsprintf(strEventName, L"__RCL2APPS__%d", GetCurrentProcessId());
		hInsuranceEvent = CreateEvent(NULL, false, false, strEventName);

		if (GetLastError () == ERROR_ALREADY_EXISTS)
		{
			SetEvent (hInsuranceEvent);
			return false; // Second copy of rcl2apps
		}
	}
	if (fdwReason == DLL_PROCESS_DETACH)
		CloseHandle (hInsuranceEvent);
	return true;
}
