/****************************************************************************
 * 0ext_c0.cpp
 *
 * Plugin module for FAR Manager 2.0
 *
 * Copyright (c) 2010 Alexey Samlyukov
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

#include "plugin.hpp"

void * __cdecl malloc(size_t size)
{
	return HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);
}

void __cdecl free(void *block)
{
	if (block) HeapFree(GetProcessHeap(),0,block);
}

struct PluginStartupInfo Info;
struct FarStandardFunctions FSF;

int WINAPI _export GetMinFarVersionW() { return MAKEFARVERSION(2,0,1666); }

void WINAPI _export SetStartupInfoW(const struct PluginStartupInfo *Info)
{
	::Info = *Info;
	if (Info->StructSize >= (int)sizeof(struct PluginStartupInfo))
	{
		FSF = *Info->FSF;
		::Info.FSF = &FSF;
	}
}

void WINAPI _export GetPluginInfoW(struct PluginInfo *Info) { Info->Flags = PF_PRELOAD; }

int WINAPI _export GetCustomDataW(const wchar_t *FilePath, wchar_t **CustomData)
{
	*CustomData = NULL;

	struct PanelInfo PInfo;
	if (!Info.Control(PANEL_ACTIVE,FCTL_GETPANELINFO,0,(LONG_PTR)&PInfo))
		return false;

	if (PInfo.PanelType==PTYPE_FILEPANEL /* && PInfo.ItemsNumber */)
	{
		WIN32_FIND_DATA wfdFindData;
		HANDLE hFind;
		bool bFile=true;
		if ((hFind=FindFirstFileW(FilePath,&wfdFindData)) != INVALID_HANDLE_VALUE)
		{
			if (wfdFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				bFile=false;
			FindClose(hFind);
		}
		const wchar_t *start=FilePath;
		while (*FilePath++)
			;
		const wchar_t *end=FilePath-1;
		while (--FilePath != start && *FilePath != L'.' && *FilePath != L'\\')
			;
		*CustomData=(wchar_t*)malloc(6*sizeof(wchar_t));

		if (*CustomData && bFile && *FilePath == L'.' && (end-FilePath-1)<=4)
		{
			FSF.sprintf(*CustomData,L"%*.*s%c",4,4,FilePath+1,0x2502);
		}
		else if (*CustomData)
		{
			FSF.sprintf(*CustomData,L"%*.*s%c",4,4,L" ",0x2502);
		}
		return true;
	}
	return false;
}

void WINAPI _export FreeCustomDataW(wchar_t *CustomData)
{
	if (CustomData)
		free(CustomData);
}
