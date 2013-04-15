#include "headers.hpp"

#include "archiver.h"
#include <initguid.h>
#include "guid.hpp"
#include "Imports.hpp"

#include "UpdateEx.hpp"
#include "lng.hpp"
#include "tinyxml.hpp"

#include "Console.hpp"
#include "CursorPos.hpp"
#include "HideCursor.hpp"
#include "TextColor.hpp"


//http://www.farmanager.com/nightly/update3.php?p=32
LPCWSTR FarRemoteSrv=L"www.farmanager.com";
LPCWSTR FarRemotePath=L"/nightly/";
LPCWSTR FarUpdateFile=L"update3.php";
LPCWSTR phpRequest=
#ifdef _WIN64
                   L"?p=64";
#else
                   L"?p=32";
#endif

//http://plugring.farmanager.com/command.php
//http://plugring.farmanager.com/download.php?fid=1082
LPCWSTR PlugRemoteSrv=L"plugring.farmanager.com";
LPCWSTR PlugRemotePath1=L"/";
LPCWSTR PlugRemotePath2=L"/download.php?fid=";
LPCWSTR PlugUpdateFile=L"command.php";

enum MODULEINFOFLAG {
	NONE     = 0x000,
	ERR      = 0x001,  // ошибка
	STD      = 0x002,  // стандартный плаг
	ANSI     = 0x004,  // ansi-плаг
	SKIP     = 0x008,  // запретим обновление
	UPD      = 0x010,  // будем обновлять
	INFO     = 0x020,  // загрузили в базу инфо об обновлении
	ARC      = 0x040,  // загрузили архив для обновления
};

struct ModuleInfo
{
	DWORD ID;  // номер в листе диалога
	GUID Guid;
	DWORD Flags;
	struct VersionInfo Version;
	wchar_t pid[64];  // plug ID
	wchar_t Title[64];
	wchar_t Description[MAX_PATH*2];
	wchar_t Author[MAX_PATH];
	wchar_t ModuleName[MAX_PATH];
	wchar_t Date[64];

	struct VersionInfo MinFarVersion;
	struct VersionInfo NewVersion;
	wchar_t ArcName[MAX_PATH];
	wchar_t NewDate[64];
	wchar_t fid[64];  // file ID
	wchar_t Downloads[64]; // количество скачиваний
};

struct IPC
{
	wchar_t FarParams[MAX_PATH*4];
	wchar_t TempDirectory[MAX_PATH];
	wchar_t Config[MAX_PATH];
	wchar_t FarUpdateList[MAX_PATH];
	wchar_t PlugUpdateList[MAX_PATH];
	//---
	DWORD DelAfterInstall;
	//---
	ModuleInfo *Modules;
	size_t CountModules;
} ipc;

void FreeModulesInfo()
{
	if (ipc.Modules) free(ipc.Modules);
	ipc.Modules=nullptr;
	ipc.CountModules=0;
}

wchar_t PluginModule[MAX_PATH];

struct OPT
{
	DWORD Auto;
	DWORD TrayNotify;
	DWORD Wait;
	DWORD ShowDisable;
	DWORD Proxy;
	DWORD SetActive;
	wchar_t ProxyName[MAX_PATH];
	wchar_t ProxyUser[MAX_PATH];
	wchar_t ProxyPass[MAX_PATH];
	wchar_t TempDirectory[MAX_PATH]; // для показа "как есть"
} opt;


enum STATUS
{
	S_NONE=0,
	S_CANTGETINSTALLINFO,
	S_CANTCREATTMP,
	S_CANTGETFARLIST,
	S_CANTGETPLUGLIST,
	S_CANTGETFARUPDINFO,
	S_CANTGETPLUGUPDINFO,
	S_UPTODATE,
	S_UPDATE,
	S_DOWNLOAD,
	S_CANTCONNECT,
	S_COMPLET,
};

enum EVENT
{
	E_LOADPLUGINS,
	E_ASKUPD,
	E_EXIT,
	E_CANTCOMPLETE
};

struct EventStruct
{
	EVENT Event;
	LPVOID Data;
	bool *Result;
};

bool NeedRestart=false;
DWORD Status=S_NONE;

SYSTEMTIME SavedTime;

HANDLE hThread=nullptr;
HANDLE hRunDll=nullptr;
HANDLE hDlg=nullptr;
HANDLE StopEvent=nullptr;
HANDLE UnlockEvent=nullptr;
HANDLE WaitEvent=nullptr;
HANDLE hNotifyThread=nullptr;

CRITICAL_SECTION cs;

PluginStartupInfo Info;
FarStandardFunctions FSF;

void SetStatus(DWORD Set)
{
	EnterCriticalSection(&cs);
	Status=Set;
	LeaveCriticalSection(&cs);
}

DWORD GetStatus()
{
	EnterCriticalSection(&cs);
	DWORD ret=Status;
	LeaveCriticalSection(&cs);
	return ret;
}

INT mprintf(LPCWSTR format,...)
{
	va_list argptr;
	va_start(argptr,format);
	wchar_t buff[1024];
	DWORD n=wvsprintf(buff,format,argptr);
	va_end(argptr);
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),buff,n,&n,nullptr);
	return n;
}

template<class T>T StringToNumber(LPCWSTR String, T &Number)
{
	Number=0;
	for(LPCWSTR p=String;p&&*p;p++)
	{
		if(*p>=L'0'&&*p<=L'9')
			Number=Number*10+(*p-L'0');
		else break;
	}
	return Number;
}

typedef BOOL (CALLBACK* DOWNLOADPROCEX)(ModuleInfo*,DWORD);

struct DownloadParam
{
	ModuleInfo *CurInfo;
	DOWNLOADPROCEX Proc;
};

char *CreatePostInfo(char *Info);


DWORD WINAPI WinInetDownloadEx(LPCWSTR strSrv, LPCWSTR strURL, LPCWSTR strFile, bool bPost=false, struct DownloadParam *Param=nullptr)
{
	DWORD err=0;

	DWORD ProxyType=INTERNET_OPEN_TYPE_DIRECT;
	if(opt.Proxy)
		ProxyType=*opt.ProxyName?INTERNET_OPEN_TYPE_PROXY:INTERNET_OPEN_TYPE_PRECONFIG;

	HINTERNET hInternet=InternetOpen(L"Mozilla/5.0 (compatible; FAR UpdateEx)",ProxyType,opt.ProxyName,nullptr,0);
	if(hInternet)
	{
		HINTERNET hConnect=InternetConnect(hInternet,strSrv,INTERNET_DEFAULT_HTTP_PORT,nullptr,nullptr,INTERNET_SERVICE_HTTP,0,0);
		if(hConnect)
		{
			if(opt.Proxy && *opt.ProxyName)
			{
				if(*opt.ProxyUser)
				{
					if (!InternetSetOption(hConnect,INTERNET_OPTION_PROXY_USERNAME,(LPVOID)opt.ProxyUser,lstrlen(opt.ProxyUser)))
					{
						err=GetLastError();
					}
				}
				if (*opt.ProxyPass)
				{
					if(!InternetSetOption(hConnect,INTERNET_OPTION_PROXY_PASSWORD,(LPVOID)opt.ProxyPass,lstrlen(opt.ProxyPass)))
					{
						err=GetLastError();
					}
				}
			}
			HTTP_VERSION_INFO httpver={1,1};
			if (!InternetSetOption(hConnect, INTERNET_OPTION_HTTP_VERSION, &httpver, sizeof(httpver)))
			{
				err=GetLastError();
			}
			HINTERNET hRequest=nullptr;

			if (bPost)
			{
				LPCWSTR AcceptTypes[] = {L"*/*",nullptr};
				hRequest=HttpOpenRequest(hConnect,L"POST",strURL,nullptr,nullptr,AcceptTypes,INTERNET_FLAG_KEEP_CONNECTION,1);
				if (hRequest)
				{
					// Формируем заголовок
					const wchar_t *hdrs=L"Content-Type: application/x-www-form-urlencoded";
					// Посылаем запрос
					char *post=nullptr;
					post=CreatePostInfo(post);
//MessageBoxA(0,post,0,MB_OK);
					if (!HttpSendRequest(hRequest,hdrs,lstrlenW(hdrs),(void*)post, lstrlenA(post)))
						err=GetLastError();
					free(post);
					Sleep(10);
				}
				else
				{
					err=GetLastError();
				}
			}
			else
			{
				hRequest=HttpOpenRequest(hConnect,L"GET",strURL,L"HTTP/1.1",nullptr,0,INTERNET_FLAG_KEEP_CONNECTION|INTERNET_FLAG_NO_CACHE_WRITE|INTERNET_FLAG_PRAGMA_NOCACHE|INTERNET_FLAG_RELOAD,1);
				if (hRequest)
				{
					if (!HttpSendRequest(hRequest,nullptr,0,nullptr,0))
						err=GetLastError();
				}
				else
				{
					err=GetLastError();
				}
			}
			if (hRequest)
			{
				DWORD StatCode=0;
				DWORD sz=sizeof(StatCode);
				if (HttpQueryInfo(hRequest,HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER,&StatCode,&sz,nullptr)
						&& StatCode!=HTTP_STATUS_NOT_FOUND && (StatCode==HTTP_STATUS_OK || StatCode==HTTP_STATUS_REDIRECT))
				{
					HANDLE hFile=CreateFile(strFile,GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
					if(hFile!=INVALID_HANDLE_VALUE)
					{
						DWORD Size=-1;
						sz=sizeof(Size);
						HttpQueryInfo(hRequest,HTTP_QUERY_CONTENT_LENGTH|HTTP_QUERY_FLAG_NUMBER,&Size,&sz,nullptr);
						DWORD ReadOk=true;
						if(Size!=GetFileSize(hFile,nullptr))
						{
							SetEndOfFile(hFile);
							UINT BytesDone=0;
							DWORD dwBytesRead;
							BYTE Data[2048];
							while((ReadOk=InternetReadFile(hRequest,Data,sizeof(Data),&dwBytesRead))!=0 && dwBytesRead)
							{
								BytesDone+=dwBytesRead;
								if (Param)
								{
									if (GetStatus()!=S_DOWNLOAD)
										break;
									if (Size && Size!=-1)
										Param->Proc(Param->CurInfo,BytesDone*100/Size);
								}
								DWORD dwWritten=0;
								if (!WriteFile(hFile,Data,dwBytesRead,&dwWritten,nullptr))
								{
									err=GetLastError();
									break;
								}
							}
						}
						CloseHandle(hFile);
						if (!ReadOk)
						{
							err=ERROR_READ_FAULT;
							DeleteFile(strFile);
						}
					}
					else
					{
						err=GetLastError();
					}
				}
				else
				{
					err=ERROR_FILE_NOT_FOUND;
				}
				InternetCloseHandle(hRequest);
			}
			InternetCloseHandle(hConnect);
		}
		else
			err=GetLastError();
		InternetCloseHandle(hInternet);
	}
	else
		err=GetLastError();

	if (Param)
	{
		if (err) { Param->CurInfo->Flags|=ERR; Param->CurInfo->Flags&= ~UPD; }
		else { Param->CurInfo->Flags|=ARC; Param->CurInfo->Flags&= ~ERR; }
		Param->Proc(Param->CurInfo,0);
	}
	return err;
}

bool DownloadFile(LPCWSTR Srv,LPCWSTR RemoteFile,LPCWSTR LocalName=nullptr,bool bPost=false)
{
	wchar_t LocalFile[MAX_PATH];
	lstrcpy(LocalFile,ipc.TempDirectory);
	lstrcat(LocalFile,LocalName?LocalName:FSF.PointToName(RemoteFile));
	return WinInetDownloadEx(Srv,RemoteFile,LocalFile,bPost)==0;
}

bool Clean()
{
	DeleteFile(PluginModule);
	RemoveDirectory(ipc.TempDirectory);
	return true;
}

bool IsTime()
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	EnterCriticalSection(&cs);
	bool Result=st.wYear!=SavedTime.wYear||st.wMonth!=SavedTime.wMonth||st.wDay!=SavedTime.wDay||(st.wHour-SavedTime.wHour)>=6;
	LeaveCriticalSection(&cs);
	return Result;
}

VOID SaveTime()
{
	EnterCriticalSection(&cs);
	GetLocalTime(&SavedTime);
	LeaveCriticalSection(&cs);
}

VOID CleanTime()
{
	EnterCriticalSection(&cs);
	memset(&SavedTime,0,sizeof(SavedTime));
	LeaveCriticalSection(&cs);
}

bool ParentIsFar()
{
	typedef struct _smPROCESS_BASIC_INFORMATION {
		LONG ExitStatus;
		PPEB PebBaseAddress;
		ULONG_PTR AffinityMask;
		LONG BasePriority;
		ULONG_PTR UniqueProcessId;
		ULONG_PTR InheritedFromUniqueProcessId;
	} smPROCESS_BASIC_INFORMATION, *smPPROCESS_BASIC_INFORMATION;

	HANDLE hFarDup;
	smPROCESS_BASIC_INFORMATION processInfo;
	DWORD ret;
	bool isFar=false;

	DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&hFarDup,0,FALSE,DUPLICATE_SAME_ACCESS);

	if (ifn.NtQueryInformationProcess(hFarDup,ProcessBasicInformation,&processInfo,sizeof(processInfo),&ret)==NO_ERROR)
	{
		HANDLE hParent=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,(DWORD)processInfo.InheritedFromUniqueProcessId);
		if (hParent)
		{
			wchar_t FileName[MAX_PATH];
			DWORD sz=MAX_PATH;
			if (GetModuleFileNameEx(hParent,nullptr,FileName,sz))
			{
				if (!FSF.LStricmp(FileName,ipc.Modules[0].ModuleName))
					isFar=true;
			}
			else if (ifn.QueryFullProcessImageNameW(hParent,0,FileName,&sz))
			{
				if (!FSF.LStricmp(FileName,ipc.Modules[0].ModuleName))
					isFar=true;
			}
			CloseHandle(hParent);
		}
	}
	CloseHandle(hFarDup);
	return isFar;
}

VOID StartUpdate(bool Thread)
{
	if (ParentIsFar())
	{
		MessageBeep(MB_ICONASTERISK);
		return;
	}

	DWORD RunDllExitCode=0;
	GetExitCodeProcess(hRunDll,&RunDllExitCode);
	if(RunDllExitCode==STILL_ACTIVE)
	{
		if (!Thread)
		{
			LPCWSTR Items[]={MSG(MName),MSG(MCantCompleteUpd),MSG(MExitFAR)};
			Info.Message(&MainGuid,&MsgCantCompleteUpdGuid, FMSG_WARNING|FMSG_MB_OK, nullptr, Items, ARRAYSIZE(Items), 0);
		}
		else
		{
			HANDLE hEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
			EventStruct es={E_CANTCOMPLETE,hEvent};
			Info.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, &es);
			WaitForSingleObject(hEvent,INFINITE);
			CloseHandle(hEvent);
		}
	}
	else if(NeedRestart)
	{
		HANDLE ProcDup;
		DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&ProcDup,0,TRUE,DUPLICATE_SAME_ACCESS);

		wchar_t cmdline[MAX_PATH];

		int NumArgs=0;
		LPWSTR *Argv=CommandLineToArgvW(GetCommandLine(), &NumArgs);
		*ipc.FarParams=0;
		for(int i=1;i<NumArgs;i++)
		{
			lstrcat(ipc.FarParams,Argv[i]);
			if(i<NumArgs-1)
				lstrcat(ipc.FarParams,L" ");
		}
		LocalFree(Argv);

		bool bSelf=false;
		for (size_t i=0; i<ipc.CountModules; i++)
		{
			if (ipc.Modules[i].Guid==MainGuid)
			{
				if (ipc.Modules[i].Flags&UPD)
				{
					if (CopyFile(ipc.Modules[i].ModuleName,PluginModule,FALSE))
						bSelf=true;
				}
				break;
			}
		}

		wchar_t WinDir[MAX_PATH];
		GetWindowsDirectory(WinDir,ARRAYSIZE(WinDir));
		BOOL IsWow64=FALSE;
		FSF.sprintf(cmdline,L"%s\\%s\\rundll32.exe \"%s\",RestartFAR %I64d %I64d",WinDir,ifn.IsWow64Process(GetCurrentProcess(),&IsWow64)&&IsWow64?L"SysWOW64":L"System32",bSelf?PluginModule:Info.ModuleName,reinterpret_cast<INT64>(ProcDup),reinterpret_cast<INT64>(&ipc));

		STARTUPINFO si={sizeof(si)};
		PROCESS_INFORMATION pi;

		BOOL Created=CreateProcess(nullptr,cmdline,nullptr,nullptr,TRUE,0,nullptr,nullptr,&si,&pi);

		if(Created)
		{
			hRunDll=pi.hProcess;
			CloseHandle(pi.hThread);
			if (!Thread)
			{
				LPCWSTR Items[]={MSG(MName),MSG(MExitFAR)};
				Info.Message(&MainGuid,&MsgExitFARGuid, FMSG_MB_OK, nullptr, Items, ARRAYSIZE(Items), 0);
			}
			else
			{
				HANDLE hEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
				EventStruct es={E_EXIT,hEvent};
				Info.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, &es);
				WaitForSingleObject(hEvent,INFINITE);
				CloseHandle(hEvent);
			}
		}
		else
		{
			if (!Thread)
			{
				LPCWSTR Items[]={MSG(MName),MSG(MCantCreateProcess)};
				Info.Message(&MainGuid,&MsgCantCreateProcessGuid, FMSG_MB_OK|FMSG_ERRORTYPE, nullptr, Items, ARRAYSIZE(Items), 0);
			}
		}
	}
}

DWORD GetUpdatesLists()
{
	DWORD Ret=S_NONE;
	CreateDirectory(ipc.TempDirectory,nullptr);
	if (GetFileAttributes(ipc.TempDirectory)==INVALID_FILE_ATTRIBUTES)
		return S_CANTCREATTMP;

	wchar_t URL[1024];
	// far
	{
		lstrcpy(URL,FarRemotePath);
		lstrcat(URL,FarUpdateFile);
		lstrcat(URL,phpRequest);
		if (!DownloadFile(FarRemoteSrv,URL,FarUpdateFile,false))
			Ret=S_CANTGETFARLIST;
	}
	// plug
	if (Ret==S_NONE)
	{
		lstrcpy(URL,PlugRemotePath1);
		lstrcat(URL,PlugUpdateFile);
		if (!DownloadFile(PlugRemoteSrv,URL,PlugUpdateFile,true))
			Ret=S_CANTGETPLUGLIST;
	}
	if (Ret)
	{
		DeleteFile(ipc.FarUpdateList);
		DeleteFile(ipc.PlugUpdateList);
	}
	return Ret;
}

bool GetCurrentModuleVersion(LPCTSTR Module,VersionInfo &vi)
{
	vi.Major=vi.Minor=vi.Revision=vi.Build=0;
	bool ret=false;
	DWORD dwHandle;
	DWORD dwSize=GetFileVersionInfoSize(Module,&dwHandle);
	if(dwSize)
	{
		LPVOID Data=malloc(dwSize);
		if(Data)
		{
			if(GetFileVersionInfo(Module,NULL,dwSize,Data))
			{
				VS_FIXEDFILEINFO *ffi;
				UINT Len;
				LPVOID lplpBuffer;
				if(VerQueryValue(Data,TEXT("\\"),&lplpBuffer,&Len))
				{
					ffi=(VS_FIXEDFILEINFO*)lplpBuffer;
					if(ffi->dwFileType==VFT_APP || ffi->dwFileType==VFT_DLL)
					{
						vi.Major=LOBYTE(HIWORD(ffi->dwFileVersionMS));
						vi.Minor=LOBYTE(LOWORD(ffi->dwFileVersionMS));
						vi.Revision=HIWORD(ffi->dwFileVersionLS);
						vi.Build=LOWORD(ffi->dwFileVersionLS);
						ret=true;
					}
				}
			}
			free(Data);
		}
	}
	return ret;
}

struct STDPLUG
{
	size_t i;
	const GUID *Guid;
	wchar_t *Log;
} StdPlug[]= {
	{0, &FarGuid,        L"http://www.farmanager.com/svn/trunk/unicode_far/changelog"},
	{1, &AlignGuid,      L"align"},
	{2, &ArcliteGuid,    L"arclite"},
	{3, &AutowrapGuid,   L"autowrap"},
	{4, &BracketsGuid,   L"brackets"},
	{5, &CompareGuid,    L"compare"},
	{6, &DrawlineGuid,   L"drawline"},
	{7, &EditcaseGuid,   L"editcase"},
	{8, &EmenuGuid,      L"emenu"},
	{9, &FarcmdsGuid,    L"farcmds"},
	{10,&FilecaseGuid,   L"filecase"},
	{11,&HlfviewerGuid,  L"hlfviewer"},
	{12,&LuamacroGuid,   L"luamacro"},
	{13,&NetworkGuid,    L"network"},
	{14,&ProclistGuid,   L"proclist"},
	{15,&TmppanelGuid,   L"tmppanel"},
	{16,&FarcolorerGuid, L"http://colorer.svn.sourceforge.net/viewvc/colorer/trunk/far3colorer/changelog"},
	{17,&NetboxGuid,     L"http://raw.github.com/michaellukashov/Far-NetBox/master/ChangeLog"}
};

bool IsStdPlug(GUID PlugGuid)
{
	for (size_t i=0; i<=17; i++)
		if (*StdPlug[i].Guid==PlugGuid)
			return true;
	return false;
}

wchar_t *GetStrFileTime(FILETIME *LastWriteTime, wchar_t *Time)
{
	SYSTEMTIME ModificTime;
	FILETIME LocalTime;
	FileTimeToLocalFileTime(LastWriteTime,&LocalTime);
	FileTimeToSystemTime(&LocalTime,&ModificTime);
	// для Time достаточно [11] !!!
	if (Time)
		FSF.sprintf(Time,L"%02d-%02d-%04d",ModificTime.wDay,ModificTime.wMonth,ModificTime.wYear);
	return Time;
}

void CopyReverseTime(wchar_t *out,const wchar_t *in)
{
	if (lstrlen(in)>=10)
	{
		memcpy(&out[0],&in[8],2*sizeof(wchar_t));
		memcpy(&out[2],&in[4],4*sizeof(wchar_t));
		memcpy(&out[6],&in[0],4*sizeof(wchar_t));
		out[10]=0;
	}
	else
		out[0]=0;
}

wchar_t *GetModuleDir(const wchar_t *Path, wchar_t *Dir)
{
	lstrcpy(Dir,Path);
	*(StrRChr(Dir,nullptr,L'\\')+1)=0;
	return Dir;
}

DWORD GetInstallModulesInfo()
{
	DWORD Ret=S_NONE;
	FreeModulesInfo();

	size_t PluginsCount=Info.PluginsControl(INVALID_HANDLE_VALUE,PCTL_GETPLUGINS,0,0);
	HANDLE *Plugins=(HANDLE*)malloc(PluginsCount*sizeof(HANDLE));
	ipc.Modules=(ModuleInfo*)malloc((PluginsCount+1)*sizeof(ModuleInfo));

	if (ipc.Modules && Plugins)
	{
		ipc.CountModules=PluginsCount+1/*Far*/;

		for (size_t i=0,j=0; i<ipc.CountModules; i++)
		{
			memset(&ipc.Modules[i],0,sizeof(ModuleInfo));
			if (i==0) // Far
			{
				Info.AdvControl(&MainGuid,ACTL_GETFARMANAGERVERSION,0,&ipc.Modules[i].Version);
				wchar_t *Far=L"Far Manager", *FarAuthor=L"Eugene Roshal & Far Group";
				lstrcpy(ipc.Modules[i].Title,Far);
				lstrcpy(ipc.Modules[i].Description,Far);
				lstrcpy(ipc.Modules[i].Author,FarAuthor);
				GetModuleFileName(nullptr,ipc.Modules[i].ModuleName,MAX_PATH);
			}
			else // plugins
			{
				Info.PluginsControl(INVALID_HANDLE_VALUE,PCTL_GETPLUGINS,PluginsCount,Plugins);
				size_t size=Info.PluginsControl(Plugins[j],PCTL_GETPLUGININFORMATION,0,0);
				FarGetPluginInformation *FGPInfo=(FarGetPluginInformation*)malloc(size);
				if (FGPInfo)
				{
					FGPInfo->StructSize=sizeof(FarGetPluginInformation);
					Info.PluginsControl(Plugins[j++],PCTL_GETPLUGININFORMATION,size,FGPInfo);
					ipc.Modules[i].Guid=FGPInfo->GInfo->Guid;
					if (FGPInfo->Flags&FPF_ANSI) ipc.Modules[i].Flags|=ANSI;
					if (IsStdPlug(FGPInfo->GInfo->Guid)) ipc.Modules[i].Flags|=STD;
					if (FGPInfo->Flags&FPF_ANSI)
						GetCurrentModuleVersion(FGPInfo->ModuleName,ipc.Modules[i].Version);
					else
						ipc.Modules[i].Version=FGPInfo->GInfo->Version;
					lstrcpy(ipc.Modules[i].Title,FGPInfo->GInfo->Title);
					lstrcpy(ipc.Modules[i].Description,FGPInfo->GInfo->Description);
					lstrcpy(ipc.Modules[i].Author,FGPInfo->GInfo->Author);
					lstrcpy(ipc.Modules[i].ModuleName,FGPInfo->ModuleName);
					free(FGPInfo);
				}
				else
				{
					Ret=S_CANTGETINSTALLINFO;
					break;
				}
			}
			HANDLE h;
			if ((h=CreateFile(ipc.Modules[i].ModuleName,0,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr))!=INVALID_HANDLE_VALUE)
			{
				FILETIME Time;
				if (GetFileTime(h,0,0,&Time)) GetStrFileTime(&Time,ipc.Modules[i].Date);
				CloseHandle(h);
			}
		}
		free(Plugins);
	}
	else Ret=S_CANTGETINSTALLINFO;

	if (Ret) FreeModulesInfo();
	return Ret;
}

bool StrToGuid(const wchar_t *Value,GUID &Guid)
{
	return (UuidFromString((unsigned short*)Value,&Guid)==RPC_S_OK)?true:false;
}

wchar_t *GuidToStr(const GUID& Guid, wchar_t *Value)
{
	if (Value)
		wsprintf(Value,L"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",Guid.Data1,Guid.Data2,Guid.Data3,Guid.Data4[0],Guid.Data4[1],Guid.Data4[2],Guid.Data4[3],Guid.Data4[4],Guid.Data4[5],Guid.Data4[6],Guid.Data4[7]);
	return Value;
}

bool CmpListGuid(wchar_t *ListGuid,GUID &Guid)
{
	bool ret=false;
	if (ListGuid&&ListGuid[0])
	{
		wchar_t guid[37];
		GuidToStr(Guid,guid);
		for (wchar_t *p=ListGuid; *p; p+=(36+1))
		{
			if (!FSF.LStrnicmp(p,guid,36))
			{
				ret=true;
				break;
			}
		}
	}
	return ret;
}

char *CreatePostInfo(char *cInfo)
{
/*
command=
<plugring>
<command code="getinfo"/>
<uids>
<uid>B076F0B0-90AE-408c-AD09-491606F09435</uid>
<uid>65642111-AA69-4B84-B4B8-9249579EC4FA</uid>
</uids>
</plugring>
*/
	wchar_t HeaderHome[]=L"command=<plugring><command code=\"getinfo\"/><uids>";
	wchar_t HeaderEnd[]=L"</uids></plugring>";
	wchar_t Body[5+36+6+1];
	wchar_t *Str=(wchar_t*)malloc((lstrlen(HeaderHome)+lstrlen(HeaderEnd)+ipc.CountModules*ARRAYSIZE(Body)+1)*sizeof(wchar_t));
	if (Str)
	{
		lstrcpy(Str,HeaderHome);
		for (size_t i=0; i<ipc.CountModules; i++)
		{
			if (!(ipc.Modules[i].Flags&ANSI) && ipc.Modules[i].Guid!=NULLGuid && !IsStdPlug(ipc.Modules[i].Guid))
			{
				wchar_t p[37];
				FSF.sprintf(Body,L"<uid>%s</uid>",GuidToStr(ipc.Modules[i].Guid,p));
				lstrcat(Str,Body);
			}
		}
		lstrcat(Str,HeaderEnd);
		size_t Size=WideCharToMultiByte(CP_UTF8,0,Str,lstrlen(Str),nullptr,0,nullptr,nullptr)+1;
		cInfo=(char*)malloc(Size);
		if (cInfo)
		{
			WideCharToMultiByte(CP_UTF8,0,Str,lstrlen(Str),cInfo,static_cast<int>(Size-1),nullptr,nullptr);
			cInfo[Size-1]=0;
		}
		free(Str);
	}
	return cInfo;
}

bool NeedUpdate(VersionInfo &Cur,VersionInfo &New, bool EqualBuild=false)
{
	return (New.Major>Cur.Major) ||
	((New.Major==Cur.Major)&&(New.Minor>Cur.Minor)) ||
	((New.Major==Cur.Major)&&(New.Minor==Cur.Minor)&&(New.Revision>Cur.Revision)) ||
	((New.Major==Cur.Major)&&(New.Minor==Cur.Minor)&&(New.Revision==Cur.Revision)&&(EqualBuild?New.Build>=Cur.Build:New.Build>Cur.Build));
}

wchar_t *CharToWChar(const char *str)
{
	wchar_t *buf=nullptr;
	if (str)
	{
		int size=MultiByteToWideChar(CP_UTF8,0,str,-1,0,0);
		buf=(wchar_t*)malloc(size*sizeof(wchar_t));
		if (buf) MultiByteToWideChar(CP_UTF8,0,str,-1,buf,size);
	}
	return buf;
}

void GetUpdModulesInfo()
{
	DWORD Ret;
	if (Ret=GetInstallModulesInfo())
	{
		SetStatus(Ret);
		return;
	}
	if (Ret=GetUpdatesLists())
	{
		SetStatus(Ret);
		return;
	}
	Ret=S_UPTODATE;

	wchar_t *ListGuid=nullptr;
	PluginSettings settings(MainGuid, Info.SettingsControl);
	int len=lstrlen(settings.Get(0,L"Skip",L""));
	if (len>36)
	{
		ListGuid=(wchar_t*)malloc((len+1)*sizeof(wchar_t));
		if (ListGuid)
			settings.Get(0,L"Skip",ListGuid,len+1,L"");
	}

/**
[info]
version="1.0"

[far]
major="3"
minor="0"
build="3167"
platform="x86"
arc="Far30b3167.x86.20130209.7z"
msi="Far30b3167.x86.20130209.msi"
date="2013-02-09"
lastchange="t-rex 08.02.2013 16:52:35 +0200 - build 3167"
**/
	// far + стандартные плаги
	{
		wchar_t Buf[MAX_PATH];
		GetPrivateProfileString(L"info",L"version",L"",Buf,ARRAYSIZE(Buf),ipc.FarUpdateList);
		if (!lstrcmp(Buf, L"1.0"))
		{
			LPCWSTR Section=L"far";
			ipc.Modules[0].NewVersion.Major=GetPrivateProfileInt(Section,L"major",-1,ipc.FarUpdateList);
			ipc.Modules[0].NewVersion.Minor=GetPrivateProfileInt(Section,L"minor",-1,ipc.FarUpdateList);
			ipc.Modules[0].NewVersion.Build=GetPrivateProfileInt(Section,L"build",-1,ipc.FarUpdateList);
			ipc.Modules[0].MinFarVersion=ipc.Modules[0].NewVersion;
			GetPrivateProfileString(Section,L"date",L"",Buf,ARRAYSIZE(Buf),ipc.FarUpdateList);
			CopyReverseTime(ipc.Modules[0].NewDate,Buf);
			GetPrivateProfileString(Section,L"arc",L"",ipc.Modules[0].ArcName,ARRAYSIZE(ipc.Modules[0].ArcName),ipc.FarUpdateList);
			// если получили имя архива и версия новее...
			if (ipc.Modules[0].ArcName[0])
			{
				ipc.Modules[0].Flags|=INFO;

				if (CmpListGuid(ListGuid,(GUID&)NULLGuid))
					ipc.Modules[0].Flags|=SKIP;
				else if (NeedUpdate(ipc.Modules[0].Version,ipc.Modules[0].NewVersion))
				{
					ipc.Modules[0].Flags|=UPD;
					Ret=S_UPDATE;
				}
			}
		}
		if (Ret!=S_UPTODATE && Ret!=S_UPDATE)
			Ret=S_CANTGETFARUPDINFO;
	}
/**
<?xml version="1.0" encoding="UTF-8"?>
<plugring>
<plugins>
<plugin plugin id="697" uid="9F25A250-45D2-45a0-90A3-5686B2A048FA" title="PicView Advanced"/>
<files>
<file id="960" plugin_id="697" flags="75" filename="PicViewAdvW_7.rar" version_major="2" version_minor="0" version_build="7" version_revision="0"
 far_major="2" far_minor="0" far_build="0" downloaded_count="213" date_added="2012-11-15 10:59:45">
</file>
<file id="1084" plugin_id="697" flags="73" filename="PicViewAdvW.x86_8.rar" version_major="3" version_minor="0" version_build="8" version_revision="0"
 far_major="3" far_minor="0" far_build="2927" downloaded_count="545" date_added="2012-12-31 11:41:38" >
</file>
<file id="1085" plugin_id="697" flags="81" filename="PicViewAdvW.x64_8.rar" version_major="3" version_minor="0" version_build="8" version_revision="0"
 far_major="3" far_minor="0" far_build="2927" downloaded_count="554" date_added="2012-12-31 11:42:31" >
</file>
</files>
</plugin>
</plugins>
**/
	// остальные плаги
	if (Ret!=S_CANTGETFARUPDINFO)
	{
		TiXmlDocument doc;
		char path[MAX_PATH];
		WideCharToMultiByte(CP_ACP,0,ipc.PlugUpdateList,-1,path,MAX_PATH,nullptr,nullptr);
		if (doc.LoadFile(path))
		{
			TiXmlElement *plugring=doc.FirstChildElement("plugring");
			if (plugring)
			{
				const TiXmlHandle root(plugring);
				for (const TiXmlElement *plugins=root.FirstChildElement("plugins").Element(); plugins; plugins=plugins->NextSiblingElement("plugins"))
				{
					const TiXmlElement *plug=plugins->FirstChildElement("plugin");
					if (plug)
					{
						ModuleInfo *CurInfo=nullptr;
						GUID plugGUID;
						wchar_t *Buf=CharToWChar(plug->Attribute("uid"));
						if (Buf)
						{
							if (StrToGuid(Buf,plugGUID))
							{
								for (size_t i=1; i<ipc.CountModules; i++)
								{
									if (ipc.Modules[i].Guid==plugGUID)
									{
										CurInfo=&ipc.Modules[i];
										break;
									}
								}
							}
							free(Buf); Buf=nullptr;
						}
						if (CurInfo)
						{
							wchar_t *Buf=CharToWChar(plug->Attribute("id"));
							if (Buf)
							{
								lstrcpy(CurInfo->pid,Buf);
								free(Buf); Buf=nullptr;
							}
							if (const TiXmlElement *filesElem=plug->FirstChildElement("files"))
							{
								for (const TiXmlElement *file=filesElem->FirstChildElement("file"); file; file=file->NextSiblingElement("file"))
								{
									Buf=CharToWChar(file->Attribute("flags"));
									if (Buf)
									{
										enum FILE_FLAG {
											BINARY = 1,
											X86 = 8,
											X64 = 16
										};
										DWORD flag=StringToNumber(Buf,flag);
										free(Buf); Buf=nullptr;
										if ((flag&BINARY) &&
#ifdef _WIN64
												(flag&X64)
#else
												(flag&X86)
#endif
										)
										{
											VersionInfo MinFarVer=MAKEFARVERSION(MIN_FAR_MAJOR_VER,MIN_FAR_MINOR_VER,0,MIN_FAR_BUILD,VS_RELEASE);
											VersionInfo CurFarVer={0,0,0,0};
											VersionInfo CurVer={0,0,0,0};

											// запрашиваем CurFarVer
											Buf=CharToWChar(file->Attribute("far_major"));
											if (Buf)
											{
												CurFarVer.Major=StringToNumber(Buf,CurFarVer.Major);
												free(Buf); Buf=nullptr;
											}
											Buf=CharToWChar(file->Attribute("far_minor"));
											if (Buf)
											{
												CurFarVer.Minor=StringToNumber(Buf,CurFarVer.Minor);
												free(Buf); Buf=nullptr;
											}
											Buf=CharToWChar(file->Attribute("far_build"));
											if (Buf)
											{
												CurFarVer.Build=StringToNumber(Buf,CurFarVer.Build);
												free(Buf); Buf=nullptr;
											}

											if (NeedUpdate(MinFarVer,CurFarVer,true))
											{
												// запрашиваем CurVer
												Buf=CharToWChar(file->Attribute("version_major"));
												if (Buf)
												{
													CurVer.Major=StringToNumber(Buf,CurVer.Major);
													free(Buf); Buf=nullptr;
												}
												Buf=CharToWChar(file->Attribute("version_minor"));
												if (Buf)
												{
													CurVer.Minor=StringToNumber(Buf,CurVer.Minor);
													free(Buf); Buf=nullptr;
												}
												Buf=CharToWChar(file->Attribute("version_revision"));
												if (Buf)
												{
													CurVer.Revision=StringToNumber(Buf,CurVer.Revision);
													free(Buf); Buf=nullptr;
												}
												Buf=CharToWChar(file->Attribute("version_build"));
												if (Buf)
												{
													CurVer.Build=StringToNumber(Buf,CurVer.Build);
													free(Buf); Buf=nullptr;
												}

												if (NeedUpdate(CurInfo->NewVersion,CurVer,true))
												{
													CurInfo->MinFarVersion=CurFarVer;
													CurInfo->NewVersion=CurVer;

													Buf=CharToWChar(file->Attribute("id"));
													if (Buf)
													{
														lstrcpy(CurInfo->fid,Buf);
														free(Buf); Buf=nullptr;
													}
													Buf=CharToWChar(file->Attribute("date_added"));
													if (Buf)
													{
														CopyReverseTime(CurInfo->NewDate,Buf);
														free(Buf); Buf=nullptr;
													}
													Buf=CharToWChar(file->Attribute("downloaded_count"));
													if (Buf)
													{
														lstrcpy(CurInfo->Downloads,Buf);
														free(Buf); Buf=nullptr;
													}
													Buf=CharToWChar(file->Attribute("filename"));
													if (Buf)
													{
														lstrcpy(CurInfo->ArcName,Buf);
														free(Buf); Buf=nullptr;
													}
													// если получили имя архива и версия новее...
													if (CurInfo->ArcName[0])
													{
														CurInfo->Flags|=INFO;
														if (CmpListGuid(ListGuid,CurInfo->Guid))
															CurInfo->Flags|=SKIP;
														else if (NeedUpdate(CurInfo->Version,CurInfo->NewVersion))
														{
															CurInfo->Flags|=UPD;
															Ret=S_UPDATE;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		if (Ret!=S_UPTODATE && Ret!=S_UPDATE)
			Ret=S_CANTGETPLUGUPDINFO;
	}
	SetStatus(Ret);
	if (ListGuid) free(ListGuid);
	DeleteFile(ipc.FarUpdateList);
	DeleteFile(ipc.PlugUpdateList);
	return;
}

enum {
	DlgBORDER = 0,  // 0
	DlgLIST,        // 1
	DlgSEP1,        // 2
	DlgDESC,        // 3
	DlgAUTHOR,      // 4
	DlgPATH,        // 5
	DlgINFO,        // 6
	DlgSEP2,        // 7
	DlgUPD,         // 8
	DlgCANCEL,      // 9
};

void MakeListItem(ModuleInfo *Cur, wchar_t *Buf, struct FarListItem &Item, DWORD Percent=-1)
{
	if ((Cur->Flags&STD) || !(Cur->Flags&INFO) || (GetStatus()==S_COMPLET && !(Cur->Flags&ARC)))
		Item.Flags|=LIF_GRAYED;
	if (Cur->Flags&UPD)
		Item.Flags|=(LIF_CHECKED|0x2b);
	else if (Cur->Flags&SKIP)
		Item.Flags|=(LIF_CHECKED|0x2d);
	wchar_t Ver[80], NewVer[80], Status[80];
	FSF.sprintf(Ver,L"%d.%d.%d.%d",Cur->Version.Major,Cur->Version.Minor,Cur->Version.Revision,Cur->Version.Build);
	if (Cur->Flags&INFO)
		FSF.sprintf(NewVer,L"%d.%d.%d.%d",Cur->NewVersion.Major,Cur->NewVersion.Minor,Cur->NewVersion.Revision,Cur->NewVersion.Build);
	else
		NewVer[0]=0;
	if (Percent!=-1 || GetStatus()==S_COMPLET)
	{
		if (Cur->Flags&ERR) lstrcpy(Status,MSG(MError));
		else if (Cur->Flags&ARC) lstrcpy(Status,MSG(MLoaded));
		else if (GetStatus()==S_COMPLET) Status[0]=0;
		else FSF.sprintf(Status,L"%3d%%",Percent);
	}
	else
		Status[0]=0;

	FSF.sprintf(Buf,L"%c%c%-15.15s %-12.12s %10.10s %c %-12.12s %10.10s %7.7s",Cur->Flags&ANSI?L'A':L' ',Cur->Flags&STD?0x2022:L' ',Cur->Title,Ver,Cur->Date,Cur->Flags&UPD?0x2192:L' ',NewVer,(Cur->Flags&INFO)?Cur->NewDate:L"",Status);
	Item.Text=Buf;
}

void MakeListItemInfo(HANDLE hDlg,void *Pos)
{
	ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,Pos);
	ModuleInfo *Cur=Tmp?*Tmp:nullptr;
	if (Cur)
	{
		wchar_t Buf[MAX_PATH];
		if (lstrlen(Cur->Description)>(80-2-2))
		{
			lstrcpyn(Buf,Cur->Description,80-2-2-3);
			lstrcat(Buf,L"...");
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgDESC,Buf);
		}
		else
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgDESC,Cur->Description);
		Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgAUTHOR,Cur->Author);
		lstrcpy(Buf,Cur->ModuleName);
		Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgPATH,FSF.TruncPathStr(Buf,80-2-2));
		if (Cur->Flags&ERR)
		{
			lstrcpy(Buf,MSG(MCantDownloadArc));
		}
		else
		{
			if (Cur->Flags&STD)
			{
				int pos=0;
				ModuleInfo **Tmp0=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void*)pos);
				ModuleInfo *Cur0=Tmp0?*Tmp0:nullptr;
				if (Cur0 && (Cur0->Flags&INFO)) FSF.sprintf(Buf,L"<%s \"%s\">",MSG(MStandard),Cur0->ArcName);
				else Buf[0]=0;
			}
			else
			{
				if (Cur->Flags&INFO) FSF.sprintf(Buf,L"<Far %d.%d.%d, downloads %s> \"%s\"",Cur->MinFarVersion.Major,Cur->MinFarVersion.Minor,Cur->MinFarVersion.Build,Cur->Downloads[0]?Cur->Downloads:L"0",Cur->ArcName);
				else lstrcpy(Buf,MSG(MIsNonInfo));
			}
		}
		if (lstrlen(Buf)>(80-2-2))
		{
			wchar_t Buf2[80];
			lstrcpyn(Buf2,Buf,80-2-2-3);
			lstrcat(Buf2,L"...");
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgINFO,Buf2);
		}
		else
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgINFO,Buf);
		Info.SendDlgMessage(hDlg,DM_REDRAW,0,0);
	}
}

bool MakeList(HANDLE hDlg,bool bSetCurPos=false)
{
	struct FarListInfo ListInfo={sizeof(FarListInfo)};
	Info.SendDlgMessage(hDlg,DM_LISTINFO,DlgLIST,&ListInfo);
	if (ListInfo.ItemsNumber)
		Info.SendDlgMessage(hDlg,DM_LISTDELETE,DlgLIST,0);

	if (!ipc.CountModules)
		return true;

	for (size_t i=0,ii=0; i<ipc.CountModules; i++)
	{
		ModuleInfo *Cur=&ipc.Modules[i];
		if ((GetStatus()!=S_COMPLET&&(Cur->Flags&INFO))||(GetStatus()==S_COMPLET&&(Cur->Flags&ARC))||opt.ShowDisable)
		{
			wchar_t Buf[MAX_PATH];
			struct FarListItem Item={};
			MakeListItem(Cur,Buf,Item);
			if (!bSetCurPos && ii==0)
				Item.Flags|=LIF_SELECTED;
			struct FarList List={sizeof(FarList)};
			List.ItemsNumber=1;
			List.Items=&Item;

			// если удачно добавили элемент...
			if (Info.SendDlgMessage(hDlg,DM_LISTADD,DlgLIST,&List))
			{
				Cur->ID=(DWORD)ii;
				// ... то ассоциируем данные с элементом листа
				struct FarListItemData Data={sizeof(FarListItemData)};
				Data.Index=ii++;
				Data.DataSize=sizeof(Cur);
				Data.Data=&Cur;
				Info.SendDlgMessage(hDlg,DM_LISTSETDATA,DlgLIST,&Data);
			}
		}
	}
	if (bSetCurPos)
	{
		FarListPos ListPos={sizeof(FarListPos)};
		ListPos.SelectPos=ListInfo.SelectPos;
		ListPos.TopPos=ListInfo.TopPos;
		Info.SendDlgMessage(hDlg,DM_LISTSETCURPOS,DlgLIST,&ListPos);
	}
	return true;
}


BOOL CALLBACK DownloadProcEx(ModuleInfo *CurInfo,DWORD Percent)
{
	if (Percent<100)
	{
		static DWORD dwTicks;
		DWORD dwNewTicks = GetTickCount();
		if (dwNewTicks - dwTicks < 500)
			return false;
		dwTicks = dwNewTicks;
	}
	struct WindowType Type={sizeof(WindowType)};
	if (Info.AdvControl(&MainGuid,ACTL_GETWINDOWTYPE,0,&Type) && Type.Type==WTYPE_DIALOG)
	{
		struct DialogInfo DInfo={sizeof(DialogInfo)};
		if (hDlg && Info.SendDlgMessage(hDlg,DM_GETDIALOGINFO,0,&DInfo) && ModulesDlgGuid==DInfo.Id)
		{
			wchar_t Buf[MAX_PATH];
			struct FarListItem Item={};
			MakeListItem(CurInfo,Buf,Item,Percent);
			intptr_t CurPos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,nullptr);
			if (CurInfo->ID==CurPos)
				Item.Flags|=LIF_SELECTED;
			struct FarListUpdate FLU={sizeof(FarListUpdate),CurInfo->ID,Item};
			if (!Info.SendDlgMessage(hDlg,DM_LISTUPDATE,DlgLIST,&FLU))
				return FALSE;
		}
	}
	return TRUE;
}

#define DN_UPDDLG DM_USER+1

bool DownloadUpdates()
{
	wchar_t URL[MAX_PATH], LocalFile[MAX_PATH];
	bool bUPD=false;
	NeedRestart=false;
	for(size_t i=0; i<ipc.CountModules; i++)
	{
		if (ipc.Modules[i].Flags&UPD)
		{
			// url архива для закачки
			lstrcpy(URL,i==0?FarRemotePath:PlugRemotePath2);
			lstrcat(URL,i==0?ipc.Modules[i].ArcName:ipc.Modules[i].fid);
			// сюда сохраним архив
			lstrcpy(LocalFile,ipc.TempDirectory);
			lstrcat(LocalFile,ipc.Modules[i].ArcName);

			struct DownloadParam Param={&ipc.Modules[i],DownloadProcEx};
			if (WinInetDownloadEx(i==0?FarRemoteSrv:PlugRemoteSrv,URL,LocalFile,false,&Param)==0)
				NeedRestart=true;
		}
		// прервали
		if (GetStatus()==S_NONE)
		{
			NeedRestart=false;
			return false;
		}
		if (ipc.Modules[i].Flags&INFO)
			bUPD=true; // что-то есть, но не отмечено
	}
	// если что-то скачали
	if (NeedRestart)
	{
		SetStatus(S_COMPLET);
		struct WindowType Type={sizeof(WindowType)};
		if (Info.AdvControl(&MainGuid,ACTL_GETWINDOWTYPE,0,&Type) && Type.Type==WTYPE_DIALOG)
		{
			struct DialogInfo DInfo={sizeof(DialogInfo)};
			if (hDlg && Info.SendDlgMessage(hDlg,DM_GETDIALOGINFO,0,&DInfo) && ModulesDlgGuid==DInfo.Id)
				Info.SendDlgMessage(hDlg,DN_UPDDLG,0,0);
		}
	}
	else
	{
		// тогда восстановим статус
		if (bUPD)
			SetStatus(S_UPTODATE);
	}
	return true;
}

bool ShowSendModulesDialog(ModuleInfo *pInfo)
{
	wchar_t Guid[37]=L"";
	bool bUnknown=true;
	if (pInfo->Guid!=FarGuid && !(pInfo->Flags&ANSI) && !(pInfo->Flags&STD))
	{
		GuidToStr(pInfo->Guid,Guid);
		bUnknown=false;
	}
	wchar_t FullFile[MAX_PATH]=L"";

	struct PanelInfo PInfo={sizeof(PanelInfo)};
	if (Info.PanelControl(PANEL_ACTIVE,FCTL_GETPANELINFO,0,&PInfo))
	{
		if (PInfo.PanelType==PTYPE_FILEPANEL && !(PInfo.Flags&PFLAGS_PLUGIN))
		{
			size_t size=Info.PanelControl(PANEL_ACTIVE,FCTL_GETCURRENTPANELITEM,0,0);
			if (size)
			{
				PluginPanelItem *PPI=(PluginPanelItem*)malloc(size);
				if (PPI)
				{
					FarGetPluginPanelItem FGPPI={sizeof(FarGetPluginPanelItem),size,PPI};
					Info.PanelControl(PANEL_ACTIVE,FCTL_GETCURRENTPANELITEM,0,&FGPPI);
					if (!(FGPPI.Item->FileAttributes&FILE_ATTRIBUTE_DIRECTORY))
					{
						if (FSF.ProcessName(L"*.7z,*.zip,*.rar",(wchar_t*)FGPPI.Item->FileName,0,PN_CMPNAMELIST))
						{
							size=Info.PanelControl(PANEL_ACTIVE,FCTL_GETPANELDIRECTORY,0,0);
							if (size)
							{
								FarPanelDirectory *buf=(FarPanelDirectory*)malloc(size);
								if (buf)
								{
									buf->StructSize=sizeof(FarPanelDirectory);
									Info.PanelControl(PANEL_ACTIVE,FCTL_GETPANELDIRECTORY,size,buf);
									lstrcpy(FullFile,buf->Name);
									if (FullFile[lstrlen(FullFile)-1]!=L'\\')
										lstrcat(FullFile,L"\\");
									lstrcat(FullFile,FGPPI.Item->FileName);
									free(buf);
								}
							}
						}
					}
					free(PPI);
				}
			}
		}
	}

	struct FarDialogItem DialogItems[] = {
		//			Type	X1	Y1	X2	Y2	Selected	History	Mask	Flags	Data	MaxLen	UserParam
		/* 0*/{DI_DOUBLEBOX,  0, 0,80,14, 0, 0, 0,     0,bUnknown?MSG(MSendUnknown):pInfo->Title,0,0},
		/* 1*/{DI_TEXT,       2, 1,20, 0, 0, 0, 0,                             0,MSG(MSendLogin),0,0},
		/* 2*/{DI_EDIT,      25, 1,50, 0, 0,L"UpdLogin",0, DIF_USELASTHISTORY|DIF_HISTORY|DIF_FOCUS,L"",0,0},
		/* 3*/{DI_PSWEDIT,   53, 1,77, 0, 0, 0, 0,                                         0,L"",0,0},
		/* 4*/{DI_TEXT,      -1, 2, 0, 0, 0, 0, 0,                             DIF_SEPARATOR,L"",0,0},
		/* 5*/{DI_TEXT,       2, 3,20, 0, 0, 0, 0,                               0,MSG(MSendUID),0,0},
		/* 6*/{DI_EDIT,      25, 3,77, 0, 0,L"UpdUID",0,                        DIF_HISTORY,Guid,0,0},
		/* 7*/{DI_TEXT,      -1, 4, 0, 0, 0, 0, 0,                             DIF_SEPARATOR,L"",0,0},
		/* 8*/{DI_TEXT,       2, 5,10, 0, 0, 0, 0,                               0,MSG(MSendVer),0,0},
		/* 9*/{DI_FIXEDIT,   11, 5,14, 0, 0, 0, L"9999",                       DIF_MASKEDIT,L"0",0,0},
		/*10*/{DI_FIXEDIT,   16, 5,19, 0, 0, 0, L"9999",                       DIF_MASKEDIT,L"0",0,0},
		/*11*/{DI_FIXEDIT,   21, 5,24, 0, 0, 0, L"9999",                       DIF_MASKEDIT,L"0",0,0},
		/*12*/{DI_FIXEDIT,   26, 5,29, 0, 0, 0, L"9999",                       DIF_MASKEDIT,L"1",0,0},
		/*13*/{DI_TEXT,      42, 5,46, 0, 0, 0, 0,                            0,MSG(MSendFarVer),0,0},
		/*14*/{DI_FIXEDIT,   48, 5,48, 0, 0, 0, L"9",                          DIF_MASKEDIT,L"3",0,0},
		/*15*/{DI_FIXEDIT,   50, 5,51, 0, 0, 0, L"99",                         DIF_MASKEDIT,L"0",0,0},
		/*16*/{DI_FIXEDIT,   53, 5,56, 0, 0, 0, L"9999",                    DIF_MASKEDIT,L"2927",0,0},
		/*17*/{DI_TEXT,      -1, 6, 0, 0, 0, 0, 0,                             DIF_SEPARATOR,L"",0,0},
		/*18*/{DI_TEXT,       2, 7,10, 0, 0, 0, 0,                              0,MSG(MSendFlag),0,0},
		/*19*/{DI_EDIT,      11, 7,39, 0, 0,L"UpdFlag",0,     DIF_USELASTHISTORY|DIF_HISTORY,L"",0,0},
		/*20*/{DI_TEXT,      42, 7,46, 0, 0, 0, 0,                               0,MSG(MSendWin),0,0},
		/*21*/{DI_EDIT,      48, 7,77, 0, 0,L"UpdWin",0,      DIF_USELASTHISTORY|DIF_HISTORY,L"",0,0},
		/*22*/{DI_TEXT,      -1, 8, 0, 0, 0, 0, 0,                             DIF_SEPARATOR,L"",0,0},
		/*23*/{DI_TEXT,       2, 9, 0, 0, 0, 0, 0,                              0,MSG(MSendFile),0,0},
		/*24*/{DI_EDIT,       2,10,77, 0, 0,L"UpdFile",0,                   DIF_HISTORY,FullFile,0,0},

		/*25*/{DI_TEXT,      -1,11, 0, 0, 0, 0, 0,                           DIF_SEPARATOR2, L"",0,0},
		/*26*/{DI_BUTTON,     0,12, 0, 0, 0, 0, 0, DIF_DEFAULTBUTTON|DIF_CENTERGROUP, MSG(MSend),0,0},
		/*27*/{DI_BUTTON,     0,12, 0, 0, 0, 0, 0,                 DIF_CENTERGROUP, MSG(MCancel),0,0}
	};

	HANDLE hDlg=Info.DialogInit(&MainGuid, &SendModulesDlgGuid,-1,-1,80,14,L"Contents",DialogItems,ARRAYSIZE(DialogItems),0,FDLG_SMALLDIALOG,0,0);

	bool ret=false;
	if (hDlg != INVALID_HANDLE_VALUE)
	{
		if (Info.DialogRun(hDlg)==26)
			ret=true;
		Info.DialogFree(hDlg);
	}
	return ret;
}

intptr_t WINAPI ShowModulesDialogProc(HANDLE hDlg,intptr_t Msg,intptr_t Param1,void *Param2)
{
	switch(Msg)
	{
		case DN_INITDIALOG:
		{
			if (GetStatus()==S_COMPLET)
				Info.SendDlgMessage(hDlg,DN_UPDDLG,0,0);
			else
				MakeList(hDlg);
			MakeListItemInfo(hDlg,0);
			break;
		}

	/************************************************************************/

		case DN_RESIZECONSOLE:
		{
			COORD c={-1,-1};
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,false,0);
			Info.SendDlgMessage(hDlg,DM_MOVEDIALOG,1,&c);
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,true,0);
			return true;
		}

		/************************************************************************/

		case DN_LISTCHANGE:
			if (Param1==DlgLIST)
			{
				MakeListItemInfo(hDlg,Param2);
				return true;
			}
			break;

		/************************************************************************/

		case DN_CONTROLINPUT:
		{
			const INPUT_RECORD* record=(const INPUT_RECORD *)Param2;
			if (record->EventType==KEY_EVENT && record->Event.KeyEvent.bKeyDown)
			{
				WORD vk=record->Event.KeyEvent.wVirtualKeyCode;
				if (IsNone(record))
				{
					if (Param1==DlgLIST)
					{
						if (vk==VK_INSERT || vk==VK_DELETE || vk==VK_ADD || vk==VK_SUBTRACT)
						{
							if (GetStatus()!=S_DOWNLOAD)
							{
								struct FarListPos FLP={sizeof(FarListPos)};
								Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,&FLP);
								ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)FLP.SelectPos);
								ModuleInfo *Cur=Tmp?*Tmp:nullptr;
								if (Cur)
								{
									struct FarListGetItem FLGI={sizeof(FarListGetItem)};
									FLGI.ItemIndex=FLP.SelectPos;
									if (Info.SendDlgMessage(hDlg,DM_LISTGETITEM,DlgLIST,&FLGI))
									{
										if (!(FLGI.Item.Flags&LIF_GRAYED))
										{
											if (vk==VK_INSERT || vk==VK_ADD)
											{
												if (LOWORD(FLGI.Item.Flags)==0x2b) FLGI.Item.Flags&= ~(LIF_CHECKED|0x2b);
												else { FLGI.Item.Flags&= ~(LIF_CHECKED|0x2d); FLGI.Item.Flags|=(LIF_CHECKED|0x2b); }
											}
											else
											{
												if (LOWORD(FLGI.Item.Flags)==0x2d) FLGI.Item.Flags&= ~(LIF_CHECKED|0x2d);
												else { FLGI.Item.Flags&= ~(LIF_CHECKED|0x2b); FLGI.Item.Flags|=(LIF_CHECKED|0x2d); }
											}
											struct FarListUpdate FLU={sizeof(FarListUpdate)};
											FLU.Index=FLGI.ItemIndex;
											FLU.Item=FLGI.Item;
											if (Info.SendDlgMessage(hDlg,DM_LISTUPDATE,DlgLIST,&FLU))
											{
												if (vk==VK_INSERT || vk==VK_ADD)
												{
													if (Cur->Flags&UPD) Cur->Flags&=~UPD;
													else { Cur->Flags|=UPD; Cur->Flags&=~SKIP; }
												}
												else
												{
													if (Cur->Flags&SKIP) Cur->Flags&=~SKIP;
													else { Cur->Flags|=SKIP; Cur->Flags&=~UPD; }
												}
												FLP.SelectPos++;
												Info.SendDlgMessage(hDlg,DM_LISTSETCURPOS,DlgLIST,&FLP);
												return true;
											}
										}
									}
								}
							}
							MessageBeep(MB_OK);
							return true;
						}
						else if (vk==VK_F3)
						{
							intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
							ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
							ModuleInfo *Cur=Tmp?*Tmp:nullptr;
							if (Cur)
							{
								for (size_t i=0; i<=17; i++)
								{
									if (*StdPlug[i].Guid==Cur->Guid && i && i<16)
									{
										wchar_t url[512]=L"http://www.farmanager.com/svn/trunk/plugins/";
										lstrcat(url,StdPlug[i].Log);
										lstrcat(url,L"/changelog");
										ShellExecute(nullptr,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);
										return true;
									}
									else if (*StdPlug[i].Guid==Cur->Guid && (i==0 || i>=16))
									{
										ShellExecute(nullptr,L"open",StdPlug[i].Log,nullptr,nullptr,SW_SHOWNORMAL);
										return true;
									}
								}
							}
							MessageBeep(MB_OK);
							return true;
						}
						else if (vk==VK_RETURN)
						{
							Info.SendDlgMessage(hDlg,DM_CLOSE,DlgUPD,0);
							return true;
						}
					}
				}
				if (IsShift(record))
				{
					if (Param1==DlgLIST)
					{
						intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
						ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
						ModuleInfo *Cur=Tmp?*Tmp:nullptr;
						if (Cur)
						{
							if (vk==VK_RETURN)
							{
								if (Cur->Guid==FarGuid || (Cur->Flags&STD))
								{
									wchar_t url[]=L"http://www.farmanager.com/nightly.php";
									ShellExecute(nullptr,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);
									return true;
								}
								else if (Cur->pid[0])
								{
									wchar_t url[128]=L"http://plugring.farmanager.com/plugin.php?pid=";
									lstrcat(url,Cur->pid);
									ShellExecute(nullptr,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);
									return true;
								}
								else
									MessageBeep(MB_OK);
							}
							else if (vk==VK_INSERT)
							{
								Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,FALSE,0);
								if (ShowSendModulesDialog(Cur))
									Info.SendDlgMessage(hDlg,DM_CLOSE,DlgCANCEL,0);
								else
									Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
								return true;
							}
						}
					}
				}
				if (IsCtrl(record))
				{
					if (Param1==DlgLIST)
					{
						if (vk==0x48) // VK_H
						{
							if (GetStatus()!=S_DOWNLOAD)
							{
								opt.ShowDisable?opt.ShowDisable=0:opt.ShowDisable=1;
								PluginSettings settings(MainGuid, Info.SettingsControl);
								settings.Set(0,L"ShowDisable",opt.ShowDisable);
								MakeList(hDlg,true);
							}
							else
								MessageBeep(MB_OK);
							return true;
						}
						else if (vk==VK_INSERT)
						{
							intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
							ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
							ModuleInfo *Cur=Tmp?*Tmp:nullptr;
							if (Cur)
								FSF.CopyToClipboard(FCT_STREAM,Cur->ModuleName);
							return true;
						}
					}
				}
				if (Param1==DlgLIST)
				{
//					struct WindowType Type={sizeof(WindowType)};
//					if (Info.AdvControl(&MainGuid,ACTL_GETWINDOWTYPE,0,&Type) && (Type.Type==WTYPE_PANELS))
					{
						PluginSettings settings(MainGuid, Info.SettingsControl);
						opt.SetActive=settings.Get(0,L"SetActive",1);
						bool isLActive=false;
						struct PanelInfo PInfo={sizeof(PanelInfo)};
						if (Info.PanelControl(PANEL_ACTIVE,FCTL_GETPANELINFO,0,&PInfo))
							isLActive=(PInfo.PanelType==PTYPE_FILEPANEL && PInfo.Flags&PFLAGS_PANELLEFT);
						bool isCtrl=IsCtrl(record);
						bool isCtrlShift=(record->Event.KeyEvent.dwControlKeyState&(RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED|SHIFT_PRESSED))!=0;

						if ((isCtrl && vk==VK_PRIOR) || (isCtrlShift && vk==VK_PRIOR)) //PgUp
						{
							intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
							ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
							ModuleInfo *Cur=Tmp?*Tmp:nullptr;
							if (Cur)
							{
								Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,FALSE,0);
								wchar_t PluginDirectory[MAX_PATH];
								GetModuleDir(Cur->ModuleName,PluginDirectory);
								FarPanelDirectory dirInfo={sizeof(FarPanelDirectory),PluginDirectory,nullptr,{0},nullptr};
								Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETPANELDIRECTORY,0,&dirInfo);
								if (opt.SetActive)
									Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETACTIVEPANEL,0,0);
								Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
								return true;
							}
						}
						else if ((isCtrl && vk==VK_NEXT) || (isCtrlShift && vk==VK_NEXT))//PgDn
						{
							intptr_t Pos=Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,DlgLIST,0);
							ModuleInfo **Tmp=(ModuleInfo **)Info.SendDlgMessage(hDlg,DM_LISTGETDATA,DlgLIST,(void *)Pos);
							ModuleInfo *Cur=Tmp?*Tmp:nullptr;
							if (Cur)
							{
								if (*Cur->ArcName)
								{
									wchar_t arc[MAX_PATH];
									lstrcpy(arc,ipc.TempDirectory);
									lstrcat(arc,Cur->ArcName);
									if(GetFileAttributes(arc)!=INVALID_FILE_ATTRIBUTES)
									{
										Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,FALSE,0);
										FarPanelDirectory dirInfo={sizeof(FarPanelDirectory),L"\\",nullptr,ArcliteGuid,arc};
										Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETPANELDIRECTORY,0,&dirInfo);
										if (opt.SetActive)
											Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETACTIVEPANEL,0,0);
										Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
										return true;
									}
								}
							}
							Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
							MessageBeep(MB_OK);
							return true;
						}
						else if ((isCtrl && vk==VK_HOME) || (isCtrlShift && vk==VK_HOME))
						{
							Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,FALSE,0);
							FarPanelDirectory dirInfo={sizeof(FarPanelDirectory),ipc.TempDirectory,nullptr,{0},nullptr};
							Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETPANELDIRECTORY,0,&dirInfo);
							if (opt.SetActive)
								Info.PanelControl(isCtrl?(isLActive?PANEL_ACTIVE:PANEL_PASSIVE):(isLActive?PANEL_PASSIVE:PANEL_ACTIVE),FCTL_SETACTIVEPANEL,0,0);
							Info.SendDlgMessage(hDlg,DM_SHOWDIALOG,TRUE,0);
							return true;
						}
					}
				}
			}
			break;
		}

		/************************************************************************/

		case DN_UPDDLG:
		{
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,false,0);
			Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,DlgUPD,(void*)MSG(MUpdate));
			MakeList(hDlg,true);
			Info.SendDlgMessage(hDlg,DM_ENABLEREDRAW,true,0);
			break;
		}
		/************************************************************************/

		case DN_CLOSE:
		{
			bool ret=false;
			if (Param1==DlgUPD)
			{
				switch(GetStatus())
				{
					case S_UPDATE:
					case S_UPTODATE:
					{
						SetStatus(S_DOWNLOAD);
						return false;
					}
					case S_COMPLET:
					{
						for(size_t i=0; i<ipc.CountModules; i++)
							if (ipc.Modules[i].Flags&UPD)
							{
								ret=true;
								break;
							}
						break;
					}
					default:
						return false;
				}
			}
			else if (Param1==DlgCANCEL || Param1==-1)
			{
				if (GetStatus()==S_DOWNLOAD)
				{
					LPCWSTR Items[]={MSG(MName),MSG(MAskCancelDownload)};
					if (Info.Message(&MainGuid,&MsgAskCancelDownloadGuid,FMSG_MB_YESNO|FMSG_WARNING,nullptr,Items,ARRAYSIZE(Items),0))
						return false;
				}
				SetStatus(S_NONE);
				ret=true;
			}
			if (ret)
			{
				wchar_t guid[38]; //36 гуид + 1 на разделитель
				wchar_t *buf=(wchar_t*)malloc(ipc.CountModules*ARRAYSIZE(guid)*sizeof(wchar_t));
				if (buf) buf[0]=0;
				bool Set=false;
				for(size_t i=0; i<ipc.CountModules; i++)
				{
					if (buf && (ipc.Modules[i].Flags&SKIP))
					{
						Set=true;
						lstrcat(buf,GuidToStr(ipc.Modules[i].Guid,guid));
						lstrcat(buf,L",");
					}
				}
				PluginSettings settings(MainGuid, Info.SettingsControl);
				if (Set)
					settings.Set(0,L"Skip",buf);
				else
					settings.DeleteValue(0,L"Skip");
				if(buf) free(buf);
			}
			return ret;
		}
	}
	return Info.DefDlgProc(hDlg,Msg,Param1,Param2);
}

bool ShowModulesDialog()
{
	struct FarDialogItem DialogItems[] = {
		//			Type	X1	Y1	X2	Y2	Selected	History	Mask	Flags	Data	MaxLen	UserParam
		/* 0*/{DI_DOUBLEBOX,  0, 0,80,25, 0, 0, 0,                             0, MSG(MAvailableUpdates),0,0},
		/* 1*/{DI_LISTBOX,    1, 1,78,16, 0, 0, 0, DIF_FOCUS|DIF_LISTNOCLOSE|DIF_LISTNOBOX,0,0,0},
		/* 2*/{DI_TEXT,      -1,17, 0, 0, 0, 0, 0,            DIF_SEPARATOR,MSG(MListButton),0,0},
		/* 3*/{DI_TEXT,       2,18,78,18,78, 0, 0,                    DIF_SHOWAMPERSAND, L"",0,0},
		/* 4*/{DI_TEXT,       2,19,78,19,78, 0, 0,       DIF_WORDWRAP|DIF_SHOWAMPERSAND, L"",0,0},
		/* 5*/{DI_TEXT,       2,20,78,20,78, 0, 0,                    DIF_SHOWAMPERSAND, L"",0,0},
		/* 6*/{DI_TEXT,       2,21,78,21,78, 0, 0,                    DIF_SHOWAMPERSAND, L"",0,0},
		/* 7*/{DI_TEXT,      -1,22, 0, 0, 0, 0, 0,                       DIF_SEPARATOR2, L"",0,0},
		/* 8*/{DI_BUTTON,     0,23, 0, 0, 0, 0, 0, DIF_DEFAULTBUTTON|DIF_CENTERGROUP, MSG(MDownload),0,0},
		/* 9*/{DI_BUTTON,     0,23, 0, 0, 0, 0, 0,             DIF_CENTERGROUP, MSG(MCancel),0,0}
	};

	bool ret=false;
	hDlg=Info.DialogInit(&MainGuid, &ModulesDlgGuid,-1,-1,80,25,L"Contents",DialogItems,ARRAYSIZE(DialogItems),0,FDLG_SMALLDIALOG,ShowModulesDialogProc,0);

	if (hDlg != INVALID_HANDLE_VALUE)
	{
		if (Info.DialogRun(hDlg)==DlgUPD)
			ret=true;
		Info.DialogFree(hDlg);
	}
	hDlg=nullptr;
	return ret;
}

VOID ReadSettings()
{
	PluginSettings settings(MainGuid, Info.SettingsControl);
	opt.Auto=settings.Get(0,L"Auto",0);
	opt.TrayNotify=settings.Get(0,L"TrayNotify",0);
	opt.Wait=settings.Get(0,L"Wait",10);
	ipc.DelAfterInstall=settings.Get(0,L"Delete",1);
	opt.Proxy=settings.Get(0,L"Proxy",0);
	settings.Get(0,L"Srv",opt.ProxyName,ARRAYSIZE(opt.ProxyName),L"");
	settings.Get(0,L"User",opt.ProxyUser,ARRAYSIZE(opt.ProxyUser),L"");
	settings.Get(0,L"Pass",opt.ProxyPass,ARRAYSIZE(opt.ProxyPass),L"");
	opt.ShowDisable=settings.Get(0,L"ShowDisable",1);

	wchar_t Buf[MAX_PATH];
	settings.Get(0,L"Dir",Buf,ARRAYSIZE(Buf),L"%TEMP%\\UpdateEx\\");
	lstrcpy(opt.TempDirectory,Buf);
	ExpandEnvironmentStrings(Buf,ipc.TempDirectory,ARRAYSIZE(ipc.TempDirectory));
	if (ipc.TempDirectory[lstrlen(ipc.TempDirectory)-1]!=L'\\') lstrcat(ipc.TempDirectory,L"\\");
	lstrcat(lstrcpy(ipc.FarUpdateList,ipc.TempDirectory),FarUpdateFile);
	lstrcat(lstrcpy(ipc.PlugUpdateList,ipc.TempDirectory),PlugUpdateFile);
	lstrcat(lstrcpy(ipc.Config,Info.ModuleName),L".config");

	lstrcat(lstrcpy(PluginModule,ipc.TempDirectory),StrRChr(Info.ModuleName,nullptr,L'\\')+1);
}

intptr_t Config()
{
	wchar_t num[64];

	struct FarDialogItem DialogItems[] = {
		//			Type	X1	Y1	X2	Y2				Selected	History	Mask	Flags	Data	MaxLen	UserParam
		/* 0*/{DI_DOUBLEBOX,  3, 1,60,13, 0, 0, 0,                  0,MSG(MName),0,0},
		/* 1*/{DI_CHECKBOX,   5, 2, 0, 0, opt.Auto, 0, 0,   DIF_FOCUS, MSG(MCfgAuto),0,0},
		/* 2*/{DI_CHECKBOX,   9, 3, 0, 0, opt.TrayNotify, 0, 0,     0, MSG(MCfgTrayNotify),0,0},
		/* 3*/{DI_FIXEDIT,    5, 4, 7, 0, 0, 0, L"999",  DIF_MASKEDIT,FSF.itoa(opt.Wait,num,10),0,0},
		/* 4*/{DI_TEXT,       9, 4,58, 0, 0, 0, 0,                  0,MSG(MCfgWait),0,0},
		/* 5*/{DI_CHECKBOX,   5, 5, 0, 0, ipc.DelAfterInstall,0, 0,DIF_3STATE,MSG(MCfgDelete),0,0},
		/* 6*/{DI_CHECKBOX,   5, 6, 0, 0, opt.Proxy, 0, 0,          0,MSG(MCfgProxy),0,0},
		/* 7*/{DI_TEXT,       9, 7,22, 0, 0,         0, 0,          0,MSG(MCfgProxySrv),0,0},
		/* 8*/{DI_EDIT,      24, 7,58, 0, 0,L"UpdCfgSrv",0, DIF_HISTORY,opt.ProxyName,0,0},
		/* 9*/{DI_TEXT,       9, 8,22, 0, 0,         0, 0,          0,MSG(MCfgPtoxyUserPass),0,0},
		/*10*/{DI_EDIT,      24, 8,41, 0, 0,L"UpdCfgUser",0,DIF_HISTORY,opt.ProxyUser,0,0},
		/*11*/{DI_PSWEDIT,   43, 8,58, 0, 0,         0, 0,          0,opt.ProxyPass,0,0},
		/*12*/{DI_TEXT,       5, 9,58, 0, 0,         0, 0,          0,MSG(MCfgDir),0,0},
		/*13*/{DI_EDIT,       5,10,58, 0, 0,L"UpdCfgDir",0, DIF_HISTORY,opt.TempDirectory,0,0},
		/*14*/{DI_TEXT,      -1,11, 0, 0, 0,         0, 0, DIF_SEPARATOR, L"",0,0},
		/*15*/{DI_BUTTON,     0,12, 0, 0, 0,         0, 0, DIF_DEFAULTBUTTON|DIF_CENTERGROUP, MSG(MOK),0,0},
		/*16*/{DI_BUTTON,     0,12, 0, 0, 0,         0, 0, DIF_CENTERGROUP, MSG(MCancel),0,0}
	};

	HANDLE hDlg=Info.DialogInit(&MainGuid, &CfgDlgGuid,-1,-1,64,15,L"Config",DialogItems,ARRAYSIZE(DialogItems),0,0,0,0);

	intptr_t ret=0;
	if (hDlg != INVALID_HANDLE_VALUE)
	{
		if (Info.DialogRun(hDlg)==15)
		{
			opt.Auto=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,1,0);
			if (opt.Auto)
				opt.TrayNotify=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,2,0);
			else
				opt.TrayNotify=0;
			opt.Wait=FSF.atoi((const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,3,0));
			if (!opt.Wait) opt.Wait=10;
			ipc.DelAfterInstall=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,5,0);
			opt.Proxy=(DWORD)Info.SendDlgMessage(hDlg,DM_GETCHECK,6,0);
			if (opt.Proxy)
			{
				lstrcpy(opt.ProxyName,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,8,0));
				lstrcpy(opt.ProxyUser,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,10,0));
				lstrcpy(opt.ProxyPass,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,11,0));
			}
			wchar_t Buf[MAX_PATH];
			lstrcpy(Buf,(const wchar_t *)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,13,0));
			if (Buf[0]==0) lstrcpy(Buf,L"%TEMP%\\UpdateEx\\");
			lstrcpy(opt.TempDirectory,Buf);
			ExpandEnvironmentStrings(Buf,ipc.TempDirectory,ARRAYSIZE(ipc.TempDirectory));
			if (ipc.TempDirectory[lstrlen(ipc.TempDirectory)-1]!=L'\\') lstrcat(ipc.TempDirectory,L"\\");
			lstrcat(lstrcpy(ipc.FarUpdateList,ipc.TempDirectory),FarUpdateFile);
			lstrcat(lstrcpy(ipc.PlugUpdateList,ipc.TempDirectory),PlugUpdateFile);

			PluginSettings settings(MainGuid, Info.SettingsControl);
			settings.Set(0,L"Auto",opt.Auto);
			settings.Set(0,L"TrayNotify",opt.TrayNotify);
			settings.Set(0,L"Wait",opt.Wait);
			settings.Set(0,L"Delete",ipc.DelAfterInstall);
			settings.Set(0,L"Proxy",opt.Proxy);
			settings.Set(0,L"Srv",opt.ProxyName);
			settings.Set(0,L"User",opt.ProxyUser);
			settings.Set(0,L"Pass",opt.ProxyPass);
			settings.Set(0,L"Dir",opt.TempDirectory);
			ret=1;
		}
		Info.DialogFree(hDlg);
	}
	return ret;
}


#define WM_TRAY_TRAYMSG WM_APP + 0x00001000
#define NOTIFY_DURATION 10000

LRESULT CALLBACK tray_wnd_proc(HWND wnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
	if (msg == WM_TRAY_TRAYMSG && l_param == WM_LBUTTONDBLCLK)
		PostQuitMessage(0);
	return DefWindowProc(wnd, msg, w_param, l_param);
}

DWORD WINAPI NotifyProc(LPVOID)
{
	HICON tray_icon=nullptr;
	HWND tray_wnd=nullptr;
	WNDCLASSEX tray_wc;
	NOTIFYICONDATA tray_icondata;

	ZeroMemory(&tray_wc, sizeof(tray_wc));
	ZeroMemory(&tray_icondata, sizeof(tray_icondata));

	tray_icon=ExtractIcon(GetModuleHandle(nullptr), Info.ModuleName, 0);

	tray_wc.cbSize=sizeof(WNDCLASSEX);
	tray_wc.style=CS_HREDRAW|CS_VREDRAW;
	tray_wc.lpfnWndProc=&tray_wnd_proc;
	tray_wc.cbClsExtra=0;
	tray_wc.cbWndExtra=0;
	tray_wc.hInstance=GetModuleHandle(nullptr);
	tray_wc.hIcon=tray_icon;
	tray_wc.hCursor=LoadCursor(nullptr, IDC_ARROW);
	tray_wc.hbrBackground=(HBRUSH)(COLOR_WINDOW + 1);
	tray_wc.lpszMenuName=nullptr;
	tray_wc.lpszClassName=L"UpdateNotifyClass";
	tray_wc.hIconSm=tray_icon;

	if (RegisterClassEx(&tray_wc))
	{
		tray_wnd=CreateWindow(tray_wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
		if (tray_wnd)
		{
			tray_icondata.cbSize=NOTIFYICONDATA_V3_SIZE;//sizeof(NOTIFYICONDATA);
			tray_icondata.uID=1;
			tray_icondata.hWnd=tray_wnd;
			tray_icondata.uFlags=NIF_ICON|NIF_MESSAGE|NIF_INFO|NIF_TIP;
			tray_icondata.hIcon=tray_icon;
			tray_icondata.uTimeout=NOTIFY_DURATION;
			tray_icondata.dwInfoFlags=NIIF_INFO|NIIF_LARGE_ICON;
			lstrcpy(tray_icondata.szInfo,MSG(MTrayNotify));
			lstrcpy(tray_icondata.szInfoTitle, MSG(MName));
			lstrcpy(tray_icondata.szTip,MSG(MTrayNotify));
			tray_icondata.uCallbackMessage=WM_TRAY_TRAYMSG;

			if (Shell_NotifyIcon(NIM_ADD, &tray_icondata))
			{
				for (;;)
				{
					MSG msg;
					if (PeekMessage(&msg,nullptr,0,0,PM_NOREMOVE))
					{
						GetMessage(&msg,nullptr,0,0);
						if (msg.message == WM_CLOSE)
							break;
					}
					if (GetStatus()!=S_DOWNLOAD)
					{
						PostQuitMessage(0);
						break;
					}
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			Shell_NotifyIcon(NIM_DELETE, &tray_icondata);
			DestroyWindow(tray_wnd);
			tray_wnd=nullptr;
		}
	}
	if (tray_icon) DestroyIcon(tray_icon);
	UnregisterClass(tray_wc.lpszClassName, GetModuleHandle(nullptr));
	CloseHandle(hNotifyThread);
	hNotifyThread=nullptr;
	return 0;
}

DWORD WINAPI ThreadProc(LPVOID /*lpParameter*/)
{
	while(WaitForSingleObject(StopEvent, 0)!=WAIT_OBJECT_0)
	{
		bool Time=false;
		Time=IsTime();
		if (Time)
		{
			ResetEvent(UnlockEvent); // защита от повторного вызова из F11
			if (opt.Auto)
			{
				HANDLE hEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
				EventStruct es={E_LOADPLUGINS,hEvent};
				Info.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, &es);
				WaitForSingleObject(hEvent,INFINITE);
				CloseHandle(hEvent);
				GetUpdModulesInfo();
			}
			else
			{
				if (WaitForSingleObject(WaitEvent,0)==WAIT_TIMEOUT)
					GetUpdModulesInfo();
			}
			if (GetStatus()==S_UPDATE || GetStatus()==S_UPTODATE)
			{
				if (WaitEvent) SetEvent(WaitEvent);
				if (GetStatus()==S_UPDATE && opt.Auto)
				{
					SetStatus(S_DOWNLOAD);
					if (opt.TrayNotify)
						hNotifyThread=CreateThread(nullptr,0,&NotifyProc,nullptr,0,0);
					DownloadUpdates();
					if (GetStatus()==S_COMPLET)
					{
						for (;;)
						{
							struct WindowType Type={sizeof(WindowType)};
							if (Info.AdvControl(&MainGuid,ACTL_GETWINDOWTYPE,0,&Type) && (Type.Type==WTYPE_PANELS || Type.Type==WTYPE_VIEWER || Type.Type==WTYPE_EDITOR))
								break;
							Sleep(1000);
						}
						bool Cancel=true;
						HANDLE hEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
						EventStruct es={E_ASKUPD,hEvent,&Cancel};
						Info.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, &es);
						WaitForSingleObject(hEvent,INFINITE);
						CloseHandle(hEvent);
						if (!Cancel)
							StartUpdate(true);
					}
				}
				SaveTime();
			}
			SetEvent(UnlockEvent);
		}
		else if (GetStatus()==S_DOWNLOAD)
		{
			ResetEvent(UnlockEvent); // защита от повторного вызова из F11
			DownloadUpdates();
			SetEvent(UnlockEvent);
		}
		Sleep(1000);
	}
	return 0;
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
	Info->Title=L"UpdateEx";
	Info->Description=L"Update plugin for Far Manager v.3.0";
	Info->Author=L"Alex Alabuzhev, Alexey Samlyukov";
}

VOID WINAPI SetStartupInfoW(const PluginStartupInfo* psInfo)
{
	ifn.Load();
	Info=*psInfo;
	FSF=*psInfo->FSF;
	Info.FSF=&FSF;
	ipc.Modules=nullptr;
	ipc.CountModules=0;
	ReadSettings();
	InitializeCriticalSection(&cs);
	StopEvent=CreateEvent(nullptr,TRUE,FALSE,nullptr);
	UnlockEvent=CreateEvent(nullptr,TRUE,FALSE,nullptr);
	hThread=CreateThread(nullptr,0,ThreadProc,nullptr,0,nullptr);
}

VOID WINAPI GetPluginInfoW(PluginInfo* pInfo)
{
	pInfo->StructSize=sizeof(PluginInfo);
	static LPCWSTR PluginMenuStrings[1],PluginConfigStrings[1];
	PluginMenuStrings[0]=MSG(MName);
	pInfo->PluginMenu.Guids = &MenuGuid;
	pInfo->PluginMenu.Strings = PluginMenuStrings;
	pInfo->PluginMenu.Count=ARRAYSIZE(PluginMenuStrings);
	PluginConfigStrings[0]=MSG(MName);
	pInfo->PluginConfig.Guids = &CfgMenuGuid;
	pInfo->PluginConfig.Strings = PluginConfigStrings;
	pInfo->PluginConfig.Count = ARRAYSIZE(PluginConfigStrings);
	pInfo->Flags=PF_EDITOR|PF_VIEWER|PF_PRELOAD;
	static LPCWSTR CommandPrefix=L"updex";
	pInfo->CommandPrefix=CommandPrefix;
}

intptr_t WINAPI ConfigureW(const ConfigureInfo* Info)
{
	return Config();
}

VOID WINAPI ExitFARW(ExitInfo* Info)
{
	if (hThread)
	{
		SetEvent(StopEvent);
		if (WaitForSingleObject(hThread,3000)== WAIT_TIMEOUT)
		{
			DWORD ec = 0;
			if (GetExitCodeThread(hThread, &ec) == STILL_ACTIVE)
				TerminateThread(hThread, ec);
			CloseHandle(hThread);
			hThread=nullptr;
		}
	}
	DeleteCriticalSection(&cs);
	CloseHandle(StopEvent);
	CloseHandle(UnlockEvent);
	if (hRunDll) CloseHandle(hRunDll);
	if (hNotifyThread)
	{
		DWORD ec = 0;
		if (GetExitCodeThread(hNotifyThread, &ec) == STILL_ACTIVE)
			TerminateThread(hNotifyThread, ec);
		CloseHandle(hNotifyThread);
		hNotifyThread=nullptr;
	}
	FreeModulesInfo();
	Clean();
}

HANDLE WINAPI OpenW(const OpenInfo* oInfo)
{
	if (WaitForSingleObject(UnlockEvent,0)==WAIT_TIMEOUT)
	{
		MessageBeep(MB_ICONASTERISK);
		return nullptr;
	}
	int Auto=opt.Auto;
	opt.Auto=0;
	CleanTime();
	WaitEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
	HANDLE hScreen=nullptr;
	for (;;)
	{
		hScreen=Info.SaveScreen(0,0,-1,-1);
		LPCWSTR Items[]={MSG(MName),MSG(MWait)};
		Info.Message(&MainGuid,&MsgWaitGuid, 0, nullptr, Items, ARRAYSIZE(Items), 0);
		// качаем листы с данными для обновления
		// т.к. сервак часто в ауте, будем ждать коннекта не более n сек
		if (WaitForSingleObject(WaitEvent,opt.Wait*1000)!=WAIT_OBJECT_0)
		{
			const wchar_t *err;
			switch(GetStatus())
			{
				case S_CANTGETINSTALLINFO:
					err=MSG(MCantGetInstallInfo);
					break;
				case S_CANTCREATTMP:
					err=MSG(MCantCreatTmp);
					break;
				case S_CANTGETFARLIST:
					err=MSG(MCantGetFarList);
					break;
				case S_CANTGETPLUGLIST:
					err=MSG(MCantGetPlugList);
					break;
				case S_CANTGETFARUPDINFO:
					err=MSG(MCantGetFarUpdInfo);
					break;
				case S_CANTGETPLUGUPDINFO:
					err=MSG(MCantGetPlugUpdInfo);
					break;
				default:
					err=MSG(MCantConnect);
					break;
			}
			Info.RestoreScreen(hScreen);
			LPCWSTR Items[]={MSG(MName),err};
			if (Info.Message(&MainGuid,&MsgErrGuid, FMSG_MB_RETRYCANCEL|FMSG_LEFTALIGN|FMSG_WARNING, nullptr, Items, ARRAYSIZE(Items), 0))
			{
				CloseHandle(WaitEvent);
				opt.Auto=Auto; // восстановим
				if (!opt.Auto) SaveTime();
				return nullptr;
			}
		}
		else
		{
			Info.RestoreScreen(hScreen);
			CloseHandle(WaitEvent);
			break;
		}
	}

	if (ShowModulesDialog())
		StartUpdate(false);

	SaveTime();
	opt.Auto=Auto; // восстановим
	Clean();

	return nullptr;
}

intptr_t WINAPI ProcessSynchroEventW(const ProcessSynchroEventInfo *pInfo)
{
	switch(pInfo->Event)
	{
		case SE_COMMONSYNCHRO:
		{
			EventStruct* es=reinterpret_cast<EventStruct*>(pInfo->Param);
			switch(es->Event)
			{
				case E_LOADPLUGINS:
				{
					SetEvent(reinterpret_cast<HANDLE>(es->Data));
					break;
				}
				case E_ASKUPD:
				{
					if (ShowModulesDialog())
						*es->Result=false;
					SetEvent(reinterpret_cast<HANDLE>(es->Data));
					break;
				}
				case E_EXIT:
				{
					LPCWSTR Items[]={MSG(MName),MSG(MExitFAR)};
					Info.Message(&MainGuid,&MsgExitFARGuid, FMSG_MB_OK, nullptr, Items, ARRAYSIZE(Items), 0);
					SetEvent(reinterpret_cast<HANDLE>(es->Data));
					break;
				}
				case E_CANTCOMPLETE:
				{
					LPCWSTR Items[]={MSG(MName),MSG(MCantCompleteUpd),MSG(MExitFAR)};
					Info.Message(&MainGuid,&MsgCantCompleteUpdGuid, FMSG_WARNING|FMSG_MB_OK, nullptr, Items, ARRAYSIZE(Items), 0);
					SetEvent(reinterpret_cast<HANDLE>(es->Data));
					break;
				}
			}
		}
		break;
	}
	return 0;
}

void Exec(bool bPreInstall, GUID *Guid, wchar_t *Config, wchar_t *CurDirectory)
{
	if (GetFileAttributes(Config)==INVALID_FILE_ATTRIBUTES)
		return;

	wchar_t exec[4096],execExp[8192];
	wchar_t guid[37]=L"";
	GetPrivateProfileString((Guid?GuidToStr(*Guid,guid):L"common"),(bPreInstall?L"PreInstall":L"PostInstall"),L"",exec,ARRAYSIZE(exec),Config);
	if(*exec)
	{
		ExpandEnvironmentStrings(exec,execExp,ARRAYSIZE(execExp));
		STARTUPINFO si={sizeof(si)};
		PROCESS_INFORMATION pi;
		{
			TextColor color(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY);
			mprintf(L"\nExecuting %-50.50s",execExp);
		}
		if(CreateProcess(nullptr,execExp,nullptr,nullptr,TRUE,0,nullptr,CurDirectory,&si,&pi))
		{
			TextColor color(FOREGROUND_GREEN|FOREGROUND_INTENSITY);
			mprintf(L"OK\n");
			WaitForSingleObject(pi.hProcess,INFINITE);
		}
		else
		{
			TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
			mprintf(L"Error %d\n",GetLastError());
		}
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
}

#include "InstallCorrection.cpp"

EXTERN_C VOID WINAPI RestartFARW(HWND,HINSTANCE,LPCWSTR lpCmd,DWORD)
{
	ifn.Load();
	INT argc=0;
	LPWSTR *argv=CommandLineToArgvW(lpCmd,&argc);
	if(argc==2)
	{
		if(!ifn.AttachConsole(ATTACH_PARENT_PROCESS))
		{
			AllocConsole();
		}
		INT_PTR ptr=StringToNumber(argv[0],ptr);
		HANDLE hFar=static_cast<HANDLE>(reinterpret_cast<LPVOID>(ptr));
		if(hFar && hFar!=INVALID_HANDLE_VALUE)
		{
			HMENU hMenu=GetSystemMenu(GetConsoleWindow(),FALSE);
			INT Count=GetMenuItemCount(hMenu);
			INT Pos=-1;
			for(int i=0;i<Count;i++)
			{
				if(GetMenuItemID(hMenu,i)==SC_CLOSE)
				{
					Pos=i;
					break;
				}
			}
			MENUITEMINFO mi={sizeof(mi),MIIM_ID,0,0,0};
			if(Pos!=-1)
			{
				SetMenuItemInfo(hMenu,Pos,MF_BYPOSITION,&mi);
			}
			INT_PTR IPCPtr=StringToNumber(argv[1],IPCPtr);
			if(ReadProcessMemory(hFar,reinterpret_cast<LPCVOID>(IPCPtr),&ipc,sizeof(IPC),nullptr))
			{
				ModuleInfo *MInfo=(ModuleInfo*)malloc(ipc.CountModules*sizeof(ModuleInfo));
				if (MInfo && ReadProcessMemory(hFar,reinterpret_cast<LPCVOID>(ipc.Modules),MInfo,ipc.CountModules*sizeof(ModuleInfo),nullptr))
				{
					WaitForSingleObject(hFar,INFINITE);

					CONSOLE_SCREEN_BUFFER_INFO csbi;
					GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&csbi);
					while(csbi.dwSize.Y--)
						mprintf(L"\n");
					CloseHandle(hFar);
					mprintf(L"\n\n\n");

					HMODULE hSevenZip=nullptr;
					wchar_t SevenZip[MAX_PATH]=L"";
					wchar_t TmpSevenZip[MAX_PATH]=L"";
					bool bDelSevenZip=false;

					for (size_t i=0; i<ipc.CountModules; i++)
					{
						if (MInfo[i].Guid==MainGuid)
						{
							GetModuleDir(MInfo[i].ModuleName,SevenZip);
							lstrcat(SevenZip,L"7z.dll");
						}
						if (MInfo[i].Guid==ArcliteGuid)
						{
							GetModuleDir(MInfo[i].ModuleName,TmpSevenZip);
							lstrcat(TmpSevenZip,L"7z.dll");
						}
					}
					if (!(hSevenZip=LoadLibrary(SevenZip)))
					{
						if (GetFileAttributes(TmpSevenZip)==INVALID_FILE_ATTRIBUTES)
						{
							if (!(hSevenZip=LoadLibrary(lstrcat(get_7z_path(HKEY_CURRENT_USER,SevenZip),L"7z.dll"))))
								hSevenZip=LoadLibrary(lstrcat(get_7z_path(HKEY_LOCAL_MACHINE,SevenZip),L"7z.dll"));
						}
						else
						{
							lstrcat(lstrcpy(SevenZip,ipc.TempDirectory),L"7z.dll");
							if (CopyFile(TmpSevenZip,SevenZip,FALSE))
							{
								bDelSevenZip=true;
								hSevenZip=LoadLibrary(SevenZip);
							}
						}
					}

					if (!hSevenZip)
					{
						TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
						mprintf(L"Can't load 7z.dll\n");
					}
					else
					{
						wchar_t CurDirectory[MAX_PATH];
						GetModuleDir(MInfo[0].ModuleName,CurDirectory);
						bool bPreInstall=true;
						Exec(bPreInstall,nullptr,ipc.Config,CurDirectory);

						for (size_t i=0; i<ipc.CountModules; i++)
						{
							if (MInfo[i].Flags&UPD)
							{
								wchar_t destpath[MAX_PATH];
								GetModuleDir(MInfo[i].ModuleName,destpath);
								// коррекция
								{
									int len=lstrlen(destpath);
									if (len>5 && !lstrcmpi(&destpath[len-5],L"\\bin\\"))
										destpath[len-4]=0;
								}
								const wchar_t *arc=MInfo[i].ArcName;
								if (*arc)
								{
									wchar_t local_arc[MAX_PATH];
									lstrcpy(local_arc,ipc.TempDirectory);
									lstrcat(local_arc,arc);

									if(GetFileAttributes(local_arc)!=INVALID_FILE_ATTRIBUTES)
									{
										bPreInstall=true;
										Exec(bPreInstall,&MInfo[i].Guid,ipc.Config,destpath);

										bool Result=false;
										wchar_t BakName[MAX_PATH];
										if (MInfo[i].Guid!=FarGuid)
										{
											lstrcat(lstrcpy(BakName,ipc.TempDirectory),StrRChr(MInfo[i].ModuleName,nullptr,L'\\')+1);
											CopyFile(MInfo[i].ModuleName,BakName,FALSE);
											DeleteFile(MInfo[i].ModuleName);
										}
										while(!Result)
										{
											TextColor color(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
											mprintf(L"\nUnpacking %-50s",arc);

											if(!extract(hSevenZip,local_arc,destpath))
											{
												TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
												mprintf(L"\nUnpack error. Retry? (Y/N) ");
												INPUT_RECORD ir={0};
												DWORD n;
												while(!(ir.EventType==KEY_EVENT && !ir.Event.KeyEvent.bKeyDown && (ir.Event.KeyEvent.wVirtualKeyCode==L'Y'||ir.Event.KeyEvent.wVirtualKeyCode==L'N')))
												{
													ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE),&ir,1,&n);
													Sleep(1);
												}
												if(ir.Event.KeyEvent.wVirtualKeyCode==L'N')
												{
													mprintf(L"\n");
													break;
												}
												mprintf(L"\n");
											}
											else
											{
												Result=true;
											}
										}
										if(Result)
										{
											{
												TextColor color(FOREGROUND_GREEN|FOREGROUND_INTENSITY);
												mprintf(L"OK\n");
											}
											if (!InstallCorrection(MInfo[i].ModuleName))
											{
												TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
												mprintf(L"Warning: %s incorrectly installed\n",MInfo[i].ArcName);
											}
											if (MInfo[i].Guid!=FarGuid)
												DeleteFile(BakName);
											if (ipc.DelAfterInstall==1 || (ipc.DelAfterInstall==2&&i==0))
												DeleteFile(local_arc);

											bPreInstall=false;
											Exec(bPreInstall,&MInfo[i].Guid,ipc.Config,destpath);
										}
										else
										{
											TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
											mprintf(L"error\n");
											if (MInfo[i].Guid!=FarGuid)
											{
												CopyFile(BakName,MInfo[i].ModuleName,FALSE);
												DeleteFile(BakName);
											}
										}
									}
								}
							}
						}
						FreeLibrary(hSevenZip);
						if (bDelSevenZip)
							DeleteFile(SevenZip);

						bPreInstall=false;
						Exec(bPreInstall,nullptr,ipc.Config,CurDirectory);
					}
					if(Pos!=-1)
					{
						mi.wID=SC_CLOSE;
						SetMenuItemInfo(hMenu,Pos,MF_BYPOSITION,&mi);
						DrawMenuBar(GetConsoleWindow());
					}
					TextColor color(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY);
					mprintf(L"\n%-60s",L"Starting Far Manager...");
					STARTUPINFO si={sizeof(si)};
					PROCESS_INFORMATION pi;
					wchar_t FarCmd[2048];
					lstrcpy(FarCmd,MInfo[0].ModuleName);
					lstrcat(FarCmd,L" ");
					lstrcat(FarCmd,ipc.FarParams);
					if(CreateProcess(nullptr,FarCmd,nullptr,nullptr,TRUE,0,nullptr,nullptr,&si,&pi))
					{
						TextColor color(FOREGROUND_GREEN|FOREGROUND_INTENSITY);
						mprintf(L"OK");
					}
					else
					{
						TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
						mprintf(L"Error %d",GetLastError());
					}
					mprintf(L"\n");
					CloseHandle(pi.hThread);
					CloseHandle(pi.hProcess);
				}
				free(MInfo);
			}
		}
	}
	LocalFree(argv);
	Clean();
}
