
bool LoadDLL(const wchar_t *Dir)
{
	bool ret=false;
	if (Dir && *Dir)
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
					if (LoadDLL(FindPath))
					{
						ret=true;
						break;
					}
				}
				else
				{
					if (!lstrcmpi(StrRChr(FindData.cFileName,nullptr,L'.'),L".dll"))
					{
						HANDLE Plugin = reinterpret_cast<HANDLE>(Info.PluginsControl(INVALID_HANDLE_VALUE, PCTL_LOADPLUGIN, PLT_PATH, FindPath));
						if (Plugin)
						{
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


DWORD WINAPI AutoloadThreadProc(LPVOID lpParam)
{
	wchar_t Dir[MAX_PATH];
	if (lpParam)
		ExpandEnvironmentStrings(L"%FARPROFILE%\\Plugins\\",Dir,ARRAYSIZE(Dir));
	else
		ExpandEnvironmentStrings(L"%FARHOME%\\Plugins\\",Dir,ARRAYSIZE(Dir));

	HANDLE hDir=CreateFile(Dir, FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED|FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (hDir!=INVALID_HANDLE_VALUE)
	{
		HANDLE hChangeEvent=CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (hChangeEvent)
		{
			if (hExitAutoloadThreadEvent)
			{
				OVERLAPPED Overlapped;
				Overlapped.hEvent=hChangeEvent;
				static char Buffer[65536];
				wchar_t FullName[MAX_PATH];

				for(;;)
				{
					DWORD n;
					if (ReadDirectoryChangesW(hDir, Buffer, sizeof(Buffer), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE, &n, &Overlapped, nullptr))
					{
						HANDLE Handles[] = {hChangeEvent, hExitAutoloadThreadEvent};
						if (WaitForMultipleObjects(ARRAYSIZE(Handles), Handles, FALSE, INFINITE) == WAIT_OBJECT_0)
						{
							DWORD BytesReturned;
							if (GetOverlappedResult(hDir, &Overlapped, &BytesReturned, FALSE) && BytesReturned)
							{
								auto NotifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(Buffer);
								for(;;)
								{
									if (WaitForSingleObject(hExitAutoloadThreadEvent, 0) != WAIT_TIMEOUT)
										break;

									wchar_t Name[MAX_PATH];
									lstrcpyn(Name,NotifyInfo->FileName,NotifyInfo->FileNameLength/sizeof(wchar_t)+1);
									*(Name+NotifyInfo->FileNameLength/sizeof(wchar_t)+1)=0;
									lstrcat(lstrcpy(FullName,Dir),Name);

									bool Continue=false;
									HANDLE Handles2[] = {UnlockEvent, hExitAutoloadThreadEvent};

									switch (NotifyInfo->Action)
									{
										case FILE_ACTION_ADDED:
											if (ipc.opt.Autoload && (GetFileAttributes(FullName)&FILE_ATTRIBUTE_DIRECTORY))
											{
												Sleep(100);
												LoadDLL(FullName);
												if (WaitForMultipleObjects(ARRAYSIZE(Handles2), Handles2, FALSE, INFINITE) == WAIT_OBJECT_0)
													GetUpdModulesInfo();
												break;
											}

										case FILE_ACTION_RENAMED_NEW_NAME:
											if (ipc.opt.Autoload && !lstrcmpi(Name + NotifyInfo->FileNameLength/sizeof(wchar_t) - 4, L".dll"))
											{
												Sleep(100);
												Info.PluginsControl(INVALID_HANDLE_VALUE, PCTL_LOADPLUGIN, PLT_PATH, FullName);
												if (WaitForMultipleObjects(ARRAYSIZE(Handles2), Handles2, FALSE, INFINITE) == WAIT_OBJECT_0)
													GetUpdModulesInfo();
											}
											break;

										case FILE_ACTION_REMOVED:
										{
											int len=lstrlen(FullName);
											for (size_t i=0; i<ipc.CountModules; i++)
											{
												if (!(CompareString(0,SORT_STRINGSORT|LINGUISTIC_IGNORECASE,ipc.Modules[i].ModuleName,len,FullName,len)-2))
												{
													lstrcpy(FullName, ipc.Modules[i].ModuleName);
													Continue=true;
													break;
												}
											}
										}

										case FILE_ACTION_RENAMED_OLD_NAME:
											if (Continue || !lstrcmpi(Name + NotifyInfo->FileNameLength/sizeof(wchar_t) - 4, L".dll"))
											{
												HANDLE Plugin = reinterpret_cast<HANDLE>(Info.PluginsControl(INVALID_HANDLE_VALUE, PCTL_FINDPLUGIN, PFM_MODULENAME, FullName));
												if (Plugin)
													Info.PluginsControl(Plugin, PCTL_UNLOADPLUGIN, 0, nullptr);
											}
											break;
									}

									if (!NotifyInfo->NextEntryOffset)
										break;

									NotifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<char*>(NotifyInfo) + NotifyInfo->NextEntryOffset);
								}
							}
						}
						else
							break;
					}
				}
			}
			CloseHandle(hChangeEvent);
		}
		CloseHandle(hDir);
	}
	return 0;
}
