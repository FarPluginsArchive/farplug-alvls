/****************************************************************************
 * AdvCmp.hpp
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

#pragma once

#include <wchar.h>
#include "plugin.hpp"
#include "farkeys.hpp"
#include "farcolor.hpp"
#include "string.hpp"
#include "libgfl.h"
#include "AdvCmpLng.hpp"        // набор констант для извлечения строк из .lng файла


/// ВАЖНО! используем данные функции, чтоб дополнительно не обнулять память
void * __cdecl malloc(size_t size);
void __cdecl free(void *block);
void * __cdecl realloc(void *block, size_t size);
#ifdef __cplusplus
void * __cdecl operator new(size_t size);
void __cdecl operator delete(void *block);
#endif
/// Подмена strncmp() (или strcmp() при n=-1)
inline int __cdecl Strncmp(const wchar_t *s1, const wchar_t *s2, int n=-1)
{
	return CompareString(0,SORT_STRINGSORT,s1,n,s2,n)-2;
}


/****************************************************************************
 * Копии стандартных структур FAR
 ****************************************************************************/
extern struct PluginStartupInfo Info;
extern struct FarStandardFunctions FSF;


/****************************************************************************
 * Текущие настройки плагина
 ****************************************************************************/
extern struct Options {
	int CmpCase,
			CmpSize,
			CmpTime,
			LowPrecisionTime,
			IgnoreTimeZone,
			CmpContents,
			OnlyTimeDiff,
			Partly,
			PartlyFull,
			PartlyKbSize,
			Ignore,
			IgnoreTemplates,
			ProcessSubfolders,
			MaxScanDepth,
			Filter,
			ProcessSelected,
			SkipSubstr,
			IgnoreMissing,
			ProcessTillFirstDiff,
			SelectedNew,
			Cache,
			CacheIgnore,
			ShowMsg,
			Sound,
			TotalProcess,
			Panel,

			ProcessHidden,
			BufSize;
	char *Buf[2];
	wchar_t *Substr;
	HANDLE hCustomFilter;
} Opt;

extern bool bBrokenByEsc;  //прекратить/прекратили операцию сравнения?
extern bool bOpenFail;     // невозможно открыть каталог/файл
extern bool bGflLoaded;    // libgfl311.dll загружена?

// информация о панели
extern struct FarPanelInfo {
	struct PanelInfo PInfo;
	HANDLE hPlugin;
	HANDLE hFilter;         // подцепим фильтр с панели
	bool bTMP;              // Tmp-панель? обрабатывается особо!
	bool bARC;              // панель-архив? обрабатывается особо!
	bool bCurFile;          // под курсором файл?
} LPanel, RPanel;

// информация об окне Фара
extern struct FarWindowsInfo {
	HWND hFarWindow;        // описатель окна фара
	SMALL_RECT Con;         // координаты консоли (символы - {0,0,79,24})
	RECT Win;               // координаты окна (точки - {0,0,1280,896})
	int TruncLen;           // оптимальная длина сообщения-сравнения
} WinInfo;

// информация
extern struct TotalCmpInfo {
	unsigned int Count;            // кол-во сравниваемых элементов
	unsigned __int64 CountSize;    // их размер
	unsigned __int64 CurCountSize; // размер сравниваемой пары
	unsigned int Proc;             // кол-во обработанных элементов
	unsigned __int64 ProcSize;     // размер обработанного
	unsigned __int64 CurProcSize;  // размер обработанного из сравниваемой пары
	unsigned int LDiff;            // кол-во отличающихся на левой панели
	unsigned int RDiff;            // кол-во отличающихся на правой панели
} CmpInfo;

// элементы для сравнения
struct DirList {
	wchar_t *Dir;           // каталог
	PluginPanelItem *PPI;   // элементы
	int ItemsNumber;        // кол-во
};

/****************************************************************************
 * Кеш сравнения "по содержимому"
 ****************************************************************************/
enum ResultCmpItemFlag {
	RCIF_EXCLUDE =1,
	RCIF_INCLUDE =2
};

// результат сравнения 2-х элементов
struct ResultCmpItem {
	DWORD   dwFullFileName[2];
	DWORD64 dwWriteTime[2];
	DWORD   dwFlags;
};

// кеш
extern struct CacheCmp {
	ResultCmpItem *RCI;
	int ItemsNumber;
} Cache;

const wchar_t *GetMsg(int MsgId);
void ErrorMsg(DWORD Title, DWORD Body);
bool YesNoMsg(DWORD Title, DWORD Body);
int DebugMsg(wchar_t *msg, wchar_t *msg2 = L" ", unsigned int i = 1000);

/****************************************************************************
 *  VisComp.dll
 ****************************************************************************/
typedef int (WINAPI *PCOMPAREFILES)(wchar_t *FileName1, wchar_t *FileName2, DWORD Options);

extern PCOMPAREFILES pCompareFiles;

/****************************************************************************
 *  libgfl311.dll
 ****************************************************************************/
typedef GFL_ERROR		(WINAPI *PGFLLIBRARYINIT)(void);
typedef void				(WINAPI *PGFLENABLELZW)(GFL_BOOL);
typedef void				(WINAPI *PGFLLIBRARYEXIT)(void);
typedef GFL_ERROR		(WINAPI *PGFLLOADBITMAPW)(const wchar_t* filename, GFL_BITMAP** bitmap, const GFL_LOAD_PARAMS* params, GFL_FILE_INFORMATION* info);
typedef GFL_INT32		(WINAPI *PGFLGETNUMBEROFFORMAT)(void);
typedef GFL_ERROR		(WINAPI *PGFLGETFORMATINFORMATIONBYINDEX)(GFL_INT32 index, GFL_FORMAT_INFORMATION* info); 
typedef void				(WINAPI *PGFLGETDEFAULTLOADPARAMS)(GFL_LOAD_PARAMS *);
typedef GFL_ERROR		(WINAPI *PGFLCHANGECOLORDEPTH)(GFL_BITMAP* src, GFL_BITMAP** dst, GFL_MODE mode, GFL_MODE_PARAMS params);
typedef GFL_ERROR		(WINAPI *PGFLROTATE)(GFL_BITMAP* src, GFL_BITMAP** dst, GFL_INT32 angle, const GFL_COLOR *background_color); 
typedef GFL_ERROR		(WINAPI *PGFLRESIZE)(GFL_BITMAP *, GFL_BITMAP **, GFL_INT32, GFL_INT32, GFL_UINT32, GFL_UINT32);
typedef void				(WINAPI *PGFLFREEBITMAP)(GFL_BITMAP *);
typedef void				(WINAPI *PGFLFREEFILEINFORMATION)(GFL_FILE_INFORMATION* info);

extern PGFLLIBRARYINIT pGflLibraryInit;
extern PGFLENABLELZW pGflEnableLZW;
extern PGFLLIBRARYEXIT pGflLibraryExit;
extern PGFLLOADBITMAPW pGflLoadBitmapW;
extern PGFLGETNUMBEROFFORMAT pGflGetNumberOfFormat;
extern PGFLGETFORMATINFORMATIONBYINDEX pGflGetFormatInformationByIndex;
extern PGFLGETDEFAULTLOADPARAMS pGflGetDefaultLoadParams;
extern PGFLCHANGECOLORDEPTH pGflChangeColorDepth;
extern PGFLROTATE pGflRotate;
extern PGFLRESIZE pGflResize;
extern PGFLFREEBITMAP pGflFreeBitmap;
extern PGFLFREEFILEINFORMATION pGflFreeFileInformation;
