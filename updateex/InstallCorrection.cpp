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
					if (!lstrcmpi(FindData.cFileName,Pattern))
					{
						lstrcpy(FileName,FindPath);
						ret=true;
						break;
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
	if (GetFileAttributes(ModuleName)==INVALID_FILE_ATTRIBUTES)
	{
		wchar_t CurDir[MAX_PATH];
		wchar_t FileName[MAX_PATH];
		if (FindFile(GetModuleDir(ModuleName,CurDir),StrRChr(ModuleName,nullptr,L'\\')+1,FileName))
		{
			wchar_t SrcDir[MAX_PATH];
			return Move(GetModuleDir(FileName,SrcDir),CurDir);
		}
		return false;
	}
	return true;
}
