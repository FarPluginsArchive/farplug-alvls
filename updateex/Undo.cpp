void ReadCache()
{
	for(size_t i=0; i<ipc.CountModules; i++)
	{
		wchar_t Buf[512], Guid[37];
		GuidToStr(ipc.Modules[i].Guid,Guid);
		GetPrivateProfileString(Guid,L"Undo",L"",Buf,ARRAYSIZE(Buf),ipc.Cache);
		if (Buf[0])
		{
			wchar_t *s=Buf;
			wchar_t *p=StrPBrk(s,L"|");

			if (p&&(p+1)) *p=0;
			else continue;

			wchar_t arc[MAX_PATH];
			lstrcat(lstrcpy(arc,ipc.TempDirectory),s);
			if (GetFileAttributes(arc)!=INVALID_FILE_ATTRIBUTES)
			{
				lstrcpy(ipc.Modules[i].Undo.ArcName,s);

				s=p+1;
				p=StrPBrk(s,L"|");
				if (p&&(p+1)) *p=0;
				else continue;

				lstrcpy(ipc.Modules[i].Undo.Date,s);

				s=p+1;
				p=StrPBrk(s,L"|");
				if (p&&(p+1)) *p=0;
				else continue;

				wchar_t *pp=s;
				for (int n=1,x=1,Number=0; pp&&*pp; pp++)
				{
					if (*pp==L'.') { n++; x=1; continue; }
					switch(n)
					{
						case 1: if (x) { ipc.Modules[i].Undo.Version.Major=StringToNumber(pp,Number); x=0; } break;
						case 2: if (x) { ipc.Modules[i].Undo.Version.Minor=StringToNumber(pp,Number); x=0; } break;
						case 3: if (x) { ipc.Modules[i].Undo.Version.Revision=StringToNumber(pp,Number); x=0; } break;
						case 4: if (x) { ipc.Modules[i].Undo.Version.Build=StringToNumber(pp,Number); x=0; } break;
					}
				}

				s=p+1;
				pp=s;
				for (int n=1,x=1,Number=0; pp&&*pp; pp++)
				{
					if (*pp==L'.') { n++; x=1; continue; }
					switch(n)
					{
						case 1: if (x) { ipc.Modules[i].Undo.MinFarVersion.Major=StringToNumber(pp,Number); x=0; } break;
						case 2: if (x) { ipc.Modules[i].Undo.MinFarVersion.Minor=StringToNumber(pp,Number); x=0; } break;
						case 3: if (x) { ipc.Modules[i].Undo.MinFarVersion.Build=StringToNumber(pp,Number); x=0; } break;
					}
				}
			}
			if (ipc.Modules[i].Undo.MinFarVersion.Major && ipc.Modules[i].Undo.ArcName[0] && ipc.Modules[i].Undo.Date[0] &&
				(ipc.Modules[i].Undo.Version.Major || ipc.Modules[i].Undo.Version.Minor || ipc.Modules[i].Undo.Version.Revision || ipc.Modules[i].Undo.Version.Build))
				ipc.Modules[i].Flags|=UNDO;
		}
	}
}

bool WriteCache(ModuleInfo *Cur)
{
	bool ret=false;
	wchar_t Guid[37], Buf[512];
	GuidToStr(Cur->Guid,Guid);
	if (ipc.opt.Mode==MODE_UNDO)
	{
		GetPrivateProfileString(Guid,L"Undo",L"",Buf,ARRAYSIZE(Buf),ipc.Cache);
		WritePrivateProfileString(Guid,L"Undo",nullptr,ipc.Cache);
		WritePrivateProfileString(Guid,L"Cur",Buf,ipc.Cache);
		ret=true;
	}
	else
	{
		GetPrivateProfileString(Guid,L"Cur",L"",Buf,ARRAYSIZE(Buf),ipc.Cache);
		if (Buf[0])
		{
			if (WritePrivateProfileString(Guid,L"Undo",Buf,ipc.Cache))
				ret=true;
		}
		wsprintf(Buf,L"%s|%s|%d.%d.%d.%d|%d.%d.%d",Cur->New.ArcName,Cur->New.Date,
								Cur->New.Version.Major,Cur->New.Version.Minor,Cur->New.Version.Revision,Cur->New.Version.Build,
								Cur->New.MinFarVersion.Major,Cur->New.MinFarVersion.Minor,Cur->New.MinFarVersion.Build);
		if (!ret)
		{
			if (WritePrivateProfileString(Guid,L"Undo",Buf,ipc.Cache))
				ret=true;
		}
		WritePrivateProfileString(Guid,L"Cur",Buf,ipc.Cache);
	}
	return ret;
}
