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
<version far_major="3" far_minor="0" far_build="2927" />
<period from="2013-01-01" to="2013-04-28" />
</plugring>
*/
	wchar_t HeaderHome[]=L"command=<plugring><command code=\"getinfo\"/>",
					HeaderEnd[]=L"</plugring>",
					HeaderUIDHome[]=L"<uids>",
					HeaderUIDEnd[]=L"</uids>",
					HeaderFar[]=L"<version far_major=\"3\" far_minor=\"0\" far_build=\"2927\" />";

	wchar_t BodyUID[5+36+6+1],
					Date[44+1];

	// выделим сразу на всё про всё
	wchar_t *Str=(wchar_t*)malloc((lstrlen(HeaderHome)+lstrlen(HeaderEnd)+
																	lstrlen(HeaderUIDHome)+lstrlen(HeaderUIDEnd)+
																	lstrlen(HeaderFar)+
																	ipc.CountModules*ARRAYSIZE(BodyUID)+
																	ARRAYSIZE(Date)+1)*sizeof(wchar_t));
	if (Str)
	{
		lstrcpy(Str,HeaderHome);
		if (ipc.opt.Mode!=MODE_NEW)
		{
			lstrcat(Str,HeaderUIDHome);
			for (size_t i=0; i<ipc.CountModules; i++)
			{
				if (ipc.Modules[i].Guid!=NULLGuid && !IsStdPlug(ipc.Modules[i].Guid))
				{
					wchar_t p[37];
					FSF.sprintf(BodyUID,L"<uid>%s</uid>",GuidToStr(ipc.Modules[i].Guid,p));
					lstrcat(Str,BodyUID);
				}
			}
			lstrcat(Str,HeaderUIDEnd);
		}
		else
		{
			lstrcat(Str,HeaderFar);
			FSF.sprintf(Date,L"<period from=\"%s\" to=\"%s\" />",ipc.opt.DateFrom,ipc.opt.DateTo);
			lstrcat(Str,Date);
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

typedef BOOL (CALLBACK* DOWNLOADPROCEX)(ModuleInfo*,DWORD);

struct DownloadParam
{
	ModuleInfo *CurInfo;
	DOWNLOADPROCEX Proc;
};

DWORD WINAPI WinInetDownloadEx(LPCWSTR strSrv, LPCWSTR strURL, LPCWSTR strFile, bool bPost=false, struct DownloadParam *Param=nullptr)
{
	DWORD err=0;

	DWORD ProxyType=INTERNET_OPEN_TYPE_DIRECT;
	if(ipc.opt.Proxy)
		ProxyType=*ipc.opt.ProxyName?INTERNET_OPEN_TYPE_PROXY:INTERNET_OPEN_TYPE_PRECONFIG;

	HINTERNET hInternet=InternetOpen(L"Mozilla/5.0 (compatible; FAR UpdateEx)",ProxyType,ipc.opt.ProxyName,nullptr,0);
	if(hInternet)
	{
		HINTERNET hConnect=InternetConnect(hInternet,strSrv,INTERNET_DEFAULT_HTTP_PORT,nullptr,nullptr,INTERNET_SERVICE_HTTP,0,0);
		if(hConnect)
		{
			if(ipc.opt.Proxy && *ipc.opt.ProxyName)
			{
				if(*ipc.opt.ProxyUser)
				{
					if (!InternetSetOption(hConnect,INTERNET_OPTION_PROXY_USERNAME,(LPVOID)ipc.opt.ProxyUser,lstrlen(ipc.opt.ProxyUser)))
					{
						err=GetLastError();
					}
				}
				if (*ipc.opt.ProxyPass)
				{
					if(!InternetSetOption(hConnect,INTERNET_OPTION_PROXY_PASSWORD,(LPVOID)ipc.opt.ProxyPass,lstrlen(ipc.opt.ProxyPass)))
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

