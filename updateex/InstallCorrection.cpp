unsigned long CRC32(unsigned long crc, const char *buf, unsigned int len)
{
	static unsigned long crc_table[256];
	if (!crc_table[1])
	{
		unsigned long c;
		int n, k;
		for (n = 0; n < 256; n++)
		{
			c = (unsigned long)n;
			for (k = 0; k < 8; k++) c = (c >> 1) ^(c & 1 ? 0xedb88320L : 0);
			crc_table[n] = c;
		}
	}
	crc = crc ^ 0xffffffffL;
	while (len-- > 0)
	{
		crc = crc_table[(crc ^(*buf++)) & 0xff] ^(crc >> 8);
	}
	return crc ^ 0xffffffffL;
}

enum
{
	CRC32_GETGLOBALINFOW   = 0x633EC0C4,
};

DWORD ExportCRC32W[] =
{
	CRC32_GETGLOBALINFOW,
};

enum PluginType
{
	NOT_PLUGIN=0,
	UNICODE_PLUGIN=1,
};

PluginType IsModulePlugin2(PBYTE hModule)
{
	DWORD dwExportAddr;
	PIMAGE_DOS_HEADER pDOSHeader = (PIMAGE_DOS_HEADER)hModule;
	PIMAGE_NT_HEADERS pPEHeader;
	{
		if (pDOSHeader->e_magic != IMAGE_DOS_SIGNATURE)
			return NOT_PLUGIN;

		pPEHeader = (PIMAGE_NT_HEADERS)&hModule[pDOSHeader->e_lfanew];

		if (pPEHeader->Signature != IMAGE_NT_SIGNATURE)
			return NOT_PLUGIN;

		if (!(pPEHeader->FileHeader.Characteristics & IMAGE_FILE_DLL))
			return NOT_PLUGIN;

		if (pPEHeader->FileHeader.Machine!=
#ifdef _WIN64
#ifdef _M_IA64
		        IMAGE_FILE_MACHINE_IA64
#else
		        IMAGE_FILE_MACHINE_AMD64
#endif
#else
		        IMAGE_FILE_MACHINE_I386
#endif
		   )
			return NOT_PLUGIN;

		dwExportAddr = pPEHeader->OptionalHeader.DataDirectory[0].VirtualAddress;

		if (!dwExportAddr)
			return NOT_PLUGIN;

		PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pPEHeader);

		for (int i = 0; i < pPEHeader->FileHeader.NumberOfSections; i++)
		{
			if ((pSection[i].VirtualAddress == dwExportAddr) ||
			        ((pSection[i].VirtualAddress <= dwExportAddr) && ((pSection[i].Misc.VirtualSize+pSection[i].VirtualAddress) > dwExportAddr)))
			{
				int nDiff = pSection[i].VirtualAddress-pSection[i].PointerToRawData;
				PIMAGE_EXPORT_DIRECTORY pExportDir = (PIMAGE_EXPORT_DIRECTORY)&hModule[dwExportAddr-nDiff];
				DWORD* pNames = (DWORD *)&hModule[pExportDir->AddressOfNames-nDiff];
				for (DWORD n = 0; n < pExportDir->NumberOfNames; n++)
				{
					const char *lpExportName = (const char *)&hModule[pNames[n]-nDiff];
					DWORD dwCRC32 = CRC32(0, lpExportName, (unsigned int)lstrlenA(lpExportName));
					for (size_t j = 0; j < ARRAYSIZE(ExportCRC32W); j++)
						if (dwCRC32 == ExportCRC32W[j])
							return UNICODE_PLUGIN;
				}
			}
		}
		return NOT_PLUGIN;
	}
	return NOT_PLUGIN;
}

bool IsModulePlugin(const wchar_t *lpModuleName)
{
	PluginType Result = NOT_PLUGIN;
	HANDLE hModuleFile = CreateFile(lpModuleName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,0,0);
	if (hModuleFile != INVALID_HANDLE_VALUE)
	{
		HANDLE hModuleMapping = CreateFileMapping(hModuleFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
		if (hModuleMapping)
		{
			PBYTE pData = (PBYTE)MapViewOfFile(hModuleMapping, FILE_MAP_READ, 0, 0, 0);
			if (pData)
			{
				Result = IsModulePlugin2(pData);
				UnmapViewOfFile(pData);
			}
			CloseHandle(hModuleMapping);
		}
		CloseHandle(hModuleFile);
	}
	return Result?true:false;
}

bool FindFile(const wchar_t *Dir, const wchar_t *Pattern, wchar_t *FileName)
{
	bool ret=false;
	if (Dir && *Dir && FileName)
	{
		wchar_t FindPath[MAX_PATH];
		lstrcpy(FindPath,Dir);
		if (FindPath[lstrlen(FindPath)-1]!=L'\\')
			lstrcat(FindPath,L"\\");
		lstrcat(FindPath,L"*");

		WIN32_FIND_DATA FindData;
		HANDLE hFind;
		if ((hFind=FindFirstFile(FindPath,&FindData)) != INVALID_HANDLE_VALUE)
		{
			do
			{
				lstrcpy(FindPath,Dir);
				if (FindPath[lstrlen(FindPath)-1]!=L'\\')
					lstrcat(FindPath,L"\\");
				lstrcat(FindPath,FindData.cFileName);

				if (FindData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
				{
					if ((FindData.cFileName[0]==L'.' && !FindData.cFileName[1]) || (FindData.cFileName[1]==L'.' && !FindData.cFileName[2]))
						continue;
					if (FindFile(FindPath,Pattern,FileName))
					{
						ret=true;
						break;
					}
				}
				else
				{
					if (!lstrcmpi(FindData.cFileName,Pattern) || !lstrcmpi(StrRChr(FindData.cFileName,nullptr,L'.'),Pattern))
					{
						if (!lstrcmpi(FindData.cFileName,L"far.exe") || IsModulePlugin(FindPath))
						{
							lstrcpy(FileName,FindPath);
							ret=true;
							break;
						}
					}
				}
			} while (FindNextFile(hFind,&FindData));
			FindClose(hFind);
		}
	}
	return ret;
}

bool Move(const wchar_t* Src, const wchar_t* Dst)
{
	bool ret=true;
	if (Src && *Src && Dst && *Dst)
	{
		wchar_t FindPath[MAX_PATH];
		lstrcpy(FindPath,Src);
		if (FindPath[lstrlen(FindPath)-1]!=L'\\')
			lstrcat(FindPath,L"\\");
		lstrcat(FindPath,L"*");

		WIN32_FIND_DATA FindData;
		HANDLE hFind=FindFirstFile(FindPath,&FindData);
		if (hFind!=INVALID_HANDLE_VALUE)
		{
			do
			{
				wchar_t SrcPath[MAX_PATH];
				lstrcpy(SrcPath,Src);
				if (SrcPath[lstrlen(SrcPath)-1]!=L'\\')
					lstrcat(SrcPath,L"\\");
				lstrcat(SrcPath,FindData.cFileName);

				wchar_t DstPath[MAX_PATH];
				lstrcpy(DstPath,Dst);
				if (DstPath[lstrlen(DstPath)-1]!=L'\\')
					lstrcat(DstPath,L"\\");
				lstrcat(DstPath,FindData.cFileName);

				if (FindData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
				{
					if (!(FindData.cFileName[0]==L'.' && !FindData.cFileName[1]) && !(FindData.cFileName[0]==L'.' && FindData.cFileName[1]==L'.' && !FindData.cFileName[2]))
					{
						if (!CreateDirectory(DstPath,nullptr))
						{
							const DWORD ec=GetLastError();
							if (ec != ERROR_ALREADY_EXISTS)
							{
								TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
								mprintf(L"Error moving \"%s\" - %d\n",FindData.cFileName,ec);
								ret=false;
							}
						}
						if (ret)
						{
							ret=Move(SrcPath,DstPath);
							RemoveDirectory(SrcPath);
						}
					}
				}
				else
				{
					if (!CopyFile(SrcPath,DstPath,FALSE))
					{
						TextColor color(FOREGROUND_RED|FOREGROUND_INTENSITY);
						mprintf(L"Error moving \"%s\" - %d\n",FindData.cFileName,GetLastError());
						ret=false;
					}
					else
					{
						mprintf(L"Moving \"%s\"\n",FindData.cFileName);
						DeleteFile(SrcPath);
					}
				}
			}
			while (ret && FindNextFile(hFind,&FindData));
			FindClose(hFind);
		}
		else ret=false;
		RemoveDirectory(Src);
	}
	else ret=false;

	return ret;
}

bool InstallCorrection(const wchar_t *ModuleName)
{
	wchar_t FullFileName[MAX_PATH], SrcDir[MAX_PATH];
	DWORD Attr=GetFileAttributes(ModuleName);
	if (Attr==INVALID_FILE_ATTRIBUTES)
	{
		wchar_t CurDir[MAX_PATH];
		if (FindFile(GetModuleDir(ModuleName,CurDir),StrRChr(ModuleName,nullptr,L'\\')+1,FullFileName))
		{
			return Move(GetModuleDir(FullFileName,SrcDir),CurDir);
		}
		return false;
	}
	else if (Attr&FILE_ATTRIBUTE_DIRECTORY) // значит новый модуль!
	{
		if (FindFile(ModuleName,L".dll",FullFileName))
		{
			GetModuleDir(FullFileName,SrcDir);
			if (lstrcmpi(SrcDir,ModuleName))
				return Move(SrcDir,ModuleName);
			else
				return true;
		}
		return false;
	}
	return true;
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
