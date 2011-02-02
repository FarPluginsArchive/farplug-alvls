/****************************************************************************
 * AdvCmp.cpp
 *
 * Plugin module for FAR Manager 2.0
 *
 * Copyright (c) 2006-2011 Alexey Samlyukov
 ****************************************************************************/
/*
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "AdvCmp.hpp"
#include "AdvCmpDlgOpt.hpp"
#include "AdvCmpProc.hpp"
#include "AdvCmpProcCur.hpp"

void * __cdecl malloc(size_t size)
{
	return HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);
}

void __cdecl free(void *block)
{
	if (block) HeapFree(GetProcessHeap(),0,block);
}

void * __cdecl realloc(void *block, size_t size)
{
	if (!size)
	{
		if (block) HeapFree(GetProcessHeap(),0,block);
		return NULL;
	}
	if (block) return HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,block,size);
	else return HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);
}
#ifdef __cplusplus
void * __cdecl operator new(size_t size)
{
	return malloc(size);
}
 void __cdecl operator delete(void *block)
{
	free(block);
}
#endif

/****************************************************************************
 * Копии стандартных структур FAR
 ****************************************************************************/
struct PluginStartupInfo Info;
struct FarStandardFunctions FSF;

/****************************************************************************
 * Набор переменных
 ****************************************************************************/
struct Options Opt;                 //Текущие настройки плагина
struct CacheCmp Cache;              //Кеш сравнения "по содержимому"
struct FarPanelInfo LPanel,RPanel;
struct TotalCmpInfo CmpInfo;
struct FarWindowsInfo WinInfo;
bool bBrokenByEsc;
bool bOpenFail;
bool bGflLoaded=false;
HMODULE GflHandle=NULL;


/****************************************************************************
 * Обёртка сервисной функции FAR: получение строки из .lng-файла
 ****************************************************************************/
const wchar_t *GetMsg(int MsgId)
{
	return Info.GetMsg(Info.ModuleNumber, MsgId);
}

/****************************************************************************
 * Показ предупреждения-ошибки с заголовком и одной строчкой
 ****************************************************************************/
void ErrorMsg(DWORD Title, DWORD Body)
{
	const wchar_t *MsgItems[]={ GetMsg(Title), GetMsg(Body), GetMsg(MOK) };
	Info.Message(Info.ModuleNumber, FMSG_WARNING, 0, MsgItems, 3, 1);
}

/****************************************************************************
 * Показ предупреждения "Yes-No" с заголовком и одной строчкой
 ****************************************************************************/
bool YesNoMsg(DWORD Title, DWORD Body)
{
	const wchar_t *MsgItems[]={ GetMsg(Title), GetMsg(Body) };
	return (!Info.Message(Info.ModuleNumber, FMSG_WARNING|FMSG_MB_YESNO, 0, MsgItems, 2, 0));
}

// Сообщение для отладки
int DebugMsg(wchar_t *msg, wchar_t *msg2, unsigned int i)
{
  wchar_t *MsgItems[] = {L"DebugMsg", L"", L"", L""};
  wchar_t buf[80]; FSF.itoa(i, buf,10);
  MsgItems[1] = msg2;
  MsgItems[2] = msg;
  MsgItems[3] = buf;
  return (!Info.Message(Info.ModuleNumber, FMSG_WARNING|FMSG_MB_OKCANCEL, 0,
                        MsgItems, sizeof(MsgItems) / sizeof(MsgItems[0]),2));
}


void FreeDirList(struct DirList *pList)
{
	if (pList->PPI)
	{
		for (int i=0; i<pList->ItemsNumber; i++)
			free((void*)pList->PPI[i].FindData.lpwszFileName);
		free(pList->PPI); pList->PPI=0;
	}
	free(pList->Dir); pList->Dir=0;
	pList->ItemsNumber=0;
}

/****************************************************************************
 * Динамическая загрузка необходимых dll
 ****************************************************************************/

///  VisComp.dll
PCOMPAREFILES pCompareFiles=NULL;

///  libgfl311.dll
PGFLLIBRARYINIT pGflLibraryInit=NULL;
PGFLENABLELZW pGflEnableLZW=NULL;
PGFLLIBRARYEXIT pGflLibraryExit=NULL;
PGFLLOADBITMAPW pGflLoadBitmapW=NULL;
PGFLGETNUMBEROFFORMAT pGflGetNumberOfFormat=NULL;
PGFLGETFORMATINFORMATIONBYINDEX pGflGetFormatInformationByIndex=NULL;
PGFLGETDEFAULTLOADPARAMS pGflGetDefaultLoadParams=NULL;
PGFLCHANGECOLORDEPTH pGflChangeColorDepth=NULL;
PGFLROTATE pGflRotate=NULL;
PGFLRESIZE pGflResize=NULL;
PGFLFREEBITMAP pGflFreeBitmap=NULL;
PGFLFREEFILEINFORMATION pGflFreeFileInformation=NULL;


bool FindFile(wchar_t *Dir, wchar_t *Pattern, string &strFileName)
{
	string strPathMask(Dir);
	strPathMask+=L"\\*";
	WIN32_FIND_DATA wfdFindData;
	HANDLE hFind;
	bool ret=false;

	if ((hFind=FindFirstFileW(strPathMask,&wfdFindData)) != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (wfdFindData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
			{
				if ((wfdFindData.cFileName[0]==L'.' && !wfdFindData.cFileName[1]) || (wfdFindData.cFileName[1]==L'.' && !wfdFindData.cFileName[2]))
					continue;
				strPathMask=Dir;
				if (strPathMask.length()>0 && strPathMask[(size_t)(strPathMask.length()-1)]!=L'\\') strPathMask+=L"\\";
				strPathMask+=wfdFindData.cFileName;
				if (FindFile(strPathMask.get(),Pattern,strFileName))
				{
					ret=true;
					break;
				}
			}
			else
			{
				if (!FSF.LStricmp(wfdFindData.cFileName,Pattern))
				{
					strFileName=Dir;
					if (strFileName.length()>0 && strFileName[(size_t)(strFileName.length()-1)]!=L'\\') strFileName+=L"\\";
					strFileName+=wfdFindData.cFileName;
					ret=true;
					break;
				}
			}
		} while (FindNextFile(hFind,&wfdFindData));
		FindClose(hFind);
	}
	return ret;
}

bool LoadVisComp(wchar_t *PlugPath)
{
	if (pCompareFiles) return true;

	string strPatchVisComp;
	if (FindFile(PlugPath,L"VisComp.dll",strPatchVisComp) &&
			Info.PluginsControl(INVALID_HANDLE_VALUE,PCTL_FORCEDLOADPLUGIN,PLT_PATH,(LONG_PTR)strPatchVisComp.get()))
	{
		pCompareFiles=(PCOMPAREFILES)GetProcAddress(GetModuleHandleW(L"VisComp.dll"),"CompareFiles");
		return true;
	}
	return false;
}

bool UnLoadGfl()
{
	if (bGflLoaded)
	{
		pGflLibraryExit();
		if (FreeLibrary(GflHandle))
			bGflLoaded=false;
		else
			return false;
	}
	return true;
}

bool LoadGfl(wchar_t *PlugPath)
{
	if (bGflLoaded) return true;

	string strPatchGfl;
	if (FindFile(PlugPath,L"libgfl311.dll",strPatchGfl))
	{
		if (!(GflHandle=LoadLibrary(strPatchGfl)))
			return false;
		if (!pGflLibraryInit)
			pGflLibraryInit=(PGFLLIBRARYINIT)GetProcAddress(GflHandle,"gflLibraryInit");
		if (!pGflEnableLZW)
			pGflEnableLZW=(PGFLENABLELZW)GetProcAddress(GflHandle,"gflEnableLZW");
		if (!pGflLibraryExit)
			pGflLibraryExit=(PGFLLIBRARYEXIT)GetProcAddress(GflHandle,"gflLibraryExit");
		if (!pGflLoadBitmapW)
			pGflLoadBitmapW=(PGFLLOADBITMAPW)GetProcAddress(GflHandle,"gflLoadBitmapW");
		if (!pGflGetNumberOfFormat)
			pGflGetNumberOfFormat=(PGFLGETNUMBEROFFORMAT)GetProcAddress(GflHandle,"gflGetNumberOfFormat");
		if (!pGflGetFormatInformationByIndex)
			pGflGetFormatInformationByIndex=(PGFLGETFORMATINFORMATIONBYINDEX)GetProcAddress(GflHandle,"gflGetFormatInformationByIndex");
		if (!pGflGetDefaultLoadParams)
			pGflGetDefaultLoadParams=(PGFLGETDEFAULTLOADPARAMS)GetProcAddress(GflHandle,"gflGetDefaultLoadParams");
		if (!pGflChangeColorDepth)
			pGflChangeColorDepth=(PGFLCHANGECOLORDEPTH)GetProcAddress(GflHandle,"gflChangeColorDepth");
		if (!pGflRotate)
			pGflRotate=(PGFLROTATE)GetProcAddress(GflHandle,"gflRotate");
		if (!pGflResize)
			pGflResize=(PGFLRESIZE)GetProcAddress(GflHandle,"gflResize");
		if (!pGflFreeBitmap)
			pGflFreeBitmap=(PGFLFREEBITMAP)GetProcAddress(GflHandle,"gflFreeBitmap");
		if (!pGflFreeFileInformation)
			pGflFreeFileInformation=(PGFLFREEFILEINFORMATION)GetProcAddress(GflHandle,"gflFreeFileInformation");

		if (!pGflLibraryInit || !pGflEnableLZW || !pGflLibraryExit || !pGflLoadBitmapW || !pGflGetNumberOfFormat || !pGflGetFormatInformationByIndex ||
				!pGflGetDefaultLoadParams || !pGflChangeColorDepth || !pGflRotate || !pGflResize || !pGflFreeBitmap || !pGflFreeFileInformation)
			return false;

		bGflLoaded=true;

		if (pGflLibraryInit()!=GFL_NO_ERROR)
			UnLoadGfl();
		if (bGflLoaded)
			pGflEnableLZW(GFL_TRUE);
	}
	return bGflLoaded;
}


/****************************************************************************
 ***************************** Exported functions ***************************
 ****************************************************************************/


/****************************************************************************
 * Эти функции плагина FAR вызывает в первую очередь
 ****************************************************************************/
// установим минимально поддерживаемую версию FARа...
int WINAPI _export GetMinFarVersionW() { return MAKEFARVERSION(2,0,1805); }

// заполним структуру PluginStartupInfo и сделаем ряд полезных действий...
void WINAPI _export SetStartupInfoW(const struct PluginStartupInfo *Info)
{
	::Info = *Info;
	if (Info->StructSize >= (int)sizeof(struct PluginStartupInfo))
	{
		FSF = *Info->FSF;
		::Info.FSF = &FSF;

		// обнулим кэш (туда будем помещать результаты сравнения)
		memset(&Cache,0,sizeof(Cache));

		wchar_t PlugPath[MAX_PATH];
		ExpandEnvironmentStringsW(L"%FARHOME%",PlugPath,(sizeof(PlugPath)/sizeof(PlugPath[0]))-9);
		wcscat(PlugPath,L"\\Plugins");
		LoadVisComp(PlugPath);
		LoadGfl(PlugPath);
	}
}


/****************************************************************************
 * Эту функцию плагина FAR вызывает во вторую очередь - заполним PluginInfo, т.е.
 * скажем FARу какие пункты добавить в "Plugin commands" и "Plugins configuration".
 ****************************************************************************/
void WINAPI _export GetPluginInfoW(struct PluginInfo *Info)
{
	static const wchar_t *PluginMenuStrings[1];
	PluginMenuStrings[0]            = GetMsg(MCompareTitle);

	Info->StructSize                = (int)sizeof(*Info);
	Info->PluginMenuStrings         = PluginMenuStrings;
	Info->PluginMenuStringsNumber   = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
}


/****************************************************************************
 * Основная функция плагина. FAR её вызывает, когда пользователь зовёт плагин
 ****************************************************************************/
HANDLE WINAPI _export OpenPluginW(int OpenFrom, INT_PTR Item)
{
	HANDLE hPlugin = INVALID_HANDLE_VALUE;
	struct PanelInfo PInfo;

	// Если не удалось запросить информацию об активной панели...
	if (!Info.Control(PANEL_ACTIVE,FCTL_GETPANELINFO,0,(LONG_PTR)&PInfo))
		return hPlugin;
	if (PInfo.Flags & PFLAGS_PANELLEFT)
	{
		Info.Control(PANEL_ACTIVE,FCTL_GETPANELINFO,0,(LONG_PTR)&LPanel.PInfo);
		LPanel.hPlugin=PANEL_ACTIVE;
	}
	else
	{
		Info.Control(PANEL_ACTIVE,FCTL_GETPANELINFO,0,(LONG_PTR)&RPanel.PInfo);
		RPanel.hPlugin=PANEL_ACTIVE;
	}
	// Если не удалось запросить информацию об пассивной панели...
	if (!Info.Control(PANEL_PASSIVE,FCTL_GETPANELINFO,0,(LONG_PTR)&PInfo))
		return hPlugin;
	if (PInfo.Flags & PFLAGS_PANELLEFT)
	{
		Info.Control(PANEL_PASSIVE,FCTL_GETPANELINFO,0,(LONG_PTR)&LPanel.PInfo);
		LPanel.hPlugin=PANEL_PASSIVE;
	}
	else
	{
		Info.Control(PANEL_PASSIVE,FCTL_GETPANELINFO,0,(LONG_PTR)&RPanel.PInfo);
		RPanel.hPlugin=PANEL_PASSIVE;
	}

	// Если панели нефайловые...
	if (LPanel.PInfo.PanelType != PTYPE_FILEPANEL || RPanel.PInfo.PanelType != PTYPE_FILEPANEL)
	{
		ErrorMsg(MCompareTitle, MFilePanelsRequired);
		return hPlugin;
	}

	LPanel.bTMP=LPanel.bARC=LPanel.bCurFile=false;
	RPanel.bTMP=RPanel.bARC=RPanel.bCurFile=false;
	struct DirList LList, RList;

	if (LPanel.PInfo.ItemsNumber)
	{
		LList.ItemsNumber=LPanel.PInfo.ItemsNumber;
		LList.PPI=(PluginPanelItem*)malloc(LList.ItemsNumber*sizeof(PluginPanelItem));
		if (LList.PPI)
		{
			for (int i=0; i<LList.ItemsNumber; i++)
			{
				PluginPanelItem *pPPI=(PluginPanelItem*)malloc(Info.Control(LPanel.hPlugin,FCTL_GETPANELITEM,i,0));
				if (pPPI)
				{
					Info.Control(LPanel.hPlugin,FCTL_GETPANELITEM,i,(LONG_PTR)pPPI);
					LList.PPI[i]=*pPPI;
					LList.PPI[i].FindData.lpwszFileName=(wchar_t*)malloc((wcslen(pPPI->FindData.lpwszFileName)+1)*sizeof(wchar_t));
					if (LList.PPI[i].FindData.lpwszFileName) wcscpy((wchar_t*)LList.PPI[i].FindData.lpwszFileName,pPPI->FindData.lpwszFileName);
					LList.PPI[i].FindData.lpwszAlternateFileName=NULL;
					LList.PPI[i].Description=NULL;
					LList.PPI[i].Owner=NULL;
					LList.PPI[i].CustomColumnData=NULL;
					LList.PPI[i].UserData=0;
					{
						if (!LPanel.bARC && LPanel.PInfo.Plugin && pPPI->CRC32)
							LPanel.bARC=true;
						if (!LPanel.bTMP && (LPanel.PInfo.Plugin && (LPanel.PInfo.Flags&PFLAGS_REALNAMES)) && wcspbrk(pPPI->FindData.lpwszFileName,L":\\/"))
							LPanel.bTMP=true;
						if (i==LPanel.PInfo.CurrentItem && !(pPPI->FindData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
							LPanel.bCurFile=true;
					}
					free(pPPI);
				}
			}
		}
		if (!LPanel.bARC)
			LPanel.bARC=LPanel.PInfo.Flags&PFLAGS_USECRC32;

		int size=Info.Control(LPanel.hPlugin,FCTL_GETPANELDIR,0,0);
		if (!LPanel.PInfo.Plugin)
		{
			wchar_t *buf=(wchar_t*)malloc(size*sizeof(wchar_t));
			if (buf) Info.Control(LPanel.hPlugin,FCTL_GETPANELDIR,size,(LONG_PTR)buf);
			size=FSF.ConvertPath(CPM_NATIVE,buf,0,0);
			LList.Dir=(wchar_t*)malloc(size*sizeof(wchar_t));
			if (LList.Dir) FSF.ConvertPath(CPM_NATIVE,buf,LList.Dir,size);
			free(buf);
		}
		else
		{
			LList.Dir=(wchar_t*)malloc(size*sizeof(wchar_t));
			if (LList.Dir) Info.Control(LPanel.hPlugin,FCTL_GETPANELDIR,size,(LONG_PTR)LList.Dir);
		}
	}

	if (RPanel.PInfo.ItemsNumber)
	{
		RList.ItemsNumber=RPanel.PInfo.ItemsNumber;
		RList.PPI=(PluginPanelItem*)malloc(RList.ItemsNumber*sizeof(PluginPanelItem));
		if (RList.PPI)
		{
			for (int i=0; i<RList.ItemsNumber; i++)
			{
				PluginPanelItem *pPPI=(PluginPanelItem*)malloc(Info.Control(RPanel.hPlugin,FCTL_GETPANELITEM,i,0));
				if (pPPI)
				{
					Info.Control(RPanel.hPlugin,FCTL_GETPANELITEM,i,(LONG_PTR)pPPI);
					RList.PPI[i]=*pPPI;
					RList.PPI[i].FindData.lpwszFileName=(wchar_t*)malloc((wcslen(pPPI->FindData.lpwszFileName)+1)*sizeof(wchar_t));
					if (RList.PPI[i].FindData.lpwszFileName) wcscpy((wchar_t*)RList.PPI[i].FindData.lpwszFileName,pPPI->FindData.lpwszFileName);
					RList.PPI[i].FindData.lpwszAlternateFileName=NULL;
					RList.PPI[i].Description=NULL;
					RList.PPI[i].Owner=NULL;
					RList.PPI[i].CustomColumnData=NULL;
					RList.PPI[i].UserData=0;
					{
						if (!RPanel.bARC && RPanel.PInfo.Plugin && pPPI->CRC32)
							RPanel.bARC=true;
						if (!RPanel.bTMP && (RPanel.PInfo.Plugin && (RPanel.PInfo.Flags&PFLAGS_REALNAMES)) && wcspbrk(pPPI->FindData.lpwszFileName,L":\\/"))
							RPanel.bTMP=true;
						if (i==RPanel.PInfo.CurrentItem && !(pPPI->FindData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
							RPanel.bCurFile=true;
					}
					free(pPPI);
				}
			}
		}
		if (!RPanel.bARC)
			RPanel.bARC=RPanel.PInfo.Flags&PFLAGS_USECRC32;

		int size=Info.Control(RPanel.hPlugin,FCTL_GETPANELDIR,0,0);
		if (!RPanel.PInfo.Plugin)
		{
			wchar_t *buf=(wchar_t*)malloc(size*sizeof(wchar_t));
			if (buf) Info.Control(RPanel.hPlugin,FCTL_GETPANELDIR,size,(LONG_PTR)buf);
			size=FSF.ConvertPath(CPM_NATIVE,buf,0,0);
			RList.Dir=(wchar_t*)malloc(size*sizeof(wchar_t));
			if (RList.Dir) FSF.ConvertPath(CPM_NATIVE,buf,RList.Dir,size);
			free(buf);
		}
		else
		{
			RList.Dir=(wchar_t*)malloc(size*sizeof(wchar_t));
			if (RList.Dir) Info.Control(RPanel.hPlugin,FCTL_GETPANELDIR,size,(LONG_PTR)RList.Dir);
		}
	}

	WinInfo.hFarWindow=(HWND)Info.AdvControl(Info.ModuleNumber,ACTL_GETFARHWND,0);
	GetClientRect(WinInfo.hFarWindow,&WinInfo.Win);
	if (Info.AdvControl(Info.ModuleNumber,ACTL_GETFARRECT,&WinInfo.Con))
	{
		WinInfo.TruncLen=WinInfo.Con.Right-WinInfo.Con.Left-20+1;
		if (WinInfo.TruncLen>MAX_PATH-2) WinInfo.TruncLen=MAX_PATH-2;
	}
	else
	{
		WinInfo.Con.Left=0;
		WinInfo.Con.Top=0;
		WinInfo.Con.Right=79;
		WinInfo.Con.Bottom=24;
		WinInfo.TruncLen=60;
	}

	memset(&CmpInfo,0,sizeof(CmpInfo));
	bBrokenByEsc=false;
	bOpenFail=false;

	class AdvCmpDlgOpt AdvCmpOpt;
	int ret=AdvCmpOpt.ShowOptDialog();

	if (ret==35) // DlgOK
	{
		DWORD dwTicks=GetTickCount();

		Info.Control(LPanel.hPlugin,FCTL_BEGINSELECTION,0,0);
		Info.Control(RPanel.hPlugin,FCTL_BEGINSELECTION,0,0);

		class AdvCmpProc AdvCmp;
		bool bDifferenceNotFound=AdvCmp.CompareDirs(&LList,&RList,true,0);

		// Отмечаем файлы и перерисовываем панели. Если нужно показываем сообщение...
		if (!bBrokenByEsc)
		{
			for (int i=0; i<LList.ItemsNumber; i++)
				Info.Control(LPanel.hPlugin,FCTL_SETSELECTION,i,Opt.Panel?(LList.PPI[i].Flags&PPIF_SELECTED):0);
			for (int i=0; i<RList.ItemsNumber; i++)
				Info.Control(RPanel.hPlugin,FCTL_SETSELECTION,i,Opt.Panel?(RList.PPI[i].Flags&PPIF_SELECTED):0);

			Info.Control(LPanel.hPlugin,FCTL_ENDSELECTION,0,0);
			Info.Control(LPanel.hPlugin,FCTL_REDRAWPANEL,0,0);
			Info.Control(RPanel.hPlugin,FCTL_ENDSELECTION,0,0);
			Info.Control(RPanel.hPlugin,FCTL_REDRAWPANEL,0,0);

			if (Opt.Sound && (GetTickCount()-dwTicks > 30000)) MessageBeep(MB_ICONASTERISK);
			Info.AdvControl(Info.ModuleNumber,ACTL_PROGRESSNOTIFY,0);
//			if (bOpenFail) ErrorMsg(MOpenErrorTitle,MOpenErrorBody);
			if (bDifferenceNotFound && Opt.ShowMsg)
			{
				const wchar_t *MsgItems[] = { GetMsg(MNoDiffTitle), GetMsg(MNoDiffBody), GetMsg(MOK) };
				Info.Message(Info.ModuleNumber,0,0,MsgItems,sizeof(MsgItems) / sizeof(MsgItems[0]),1);
			}
			else if (!bDifferenceNotFound && !Opt.Panel)
				AdvCmp.ShowCmpDialog(&LList,&RList);
		}
	}
	else if (ret==36) // DlgUNDERCURSOR
	{
		class AdvCmpProcCur AdvCmpCur;
		AdvCmpCur.CompareCurFile(&LList,&RList);
	}

	FreeDirList(&LList);
	FreeDirList(&RList);

	return hPlugin;
}

/****************************************************************************
 * Эту функцию FAR вызывает перед выгрузкой плагина
 ****************************************************************************/
void WINAPI _export ExitFARW(void)
{
	//Освободим память в случае выгрузки плагина
	if (Cache.RCI)
		free(Cache.RCI);
	Cache.RCI=0;
	Cache.ItemsNumber=0;

	UnLoadGfl();
}
