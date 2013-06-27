void ReadCache()
{
	for(size_t i=0; i<ipc.CountModules; i++)
	{
		wchar_t Buf[64], Guid[37];
		GuidToStr(ipc.Modules[i].Guid,Guid);

		GetPrivateProfileString(Guid,L"ArcName",L"",ipc.Modules[i].Undo.ArcName,ARRAYSIZE(ipc.Modules[i].Undo.ArcName),ipc.Cache);
		GetPrivateProfileString(Guid,L"Date",L"",ipc.Modules[i].Undo.Date,ARRAYSIZE(ipc.Modules[i].Undo.Date),ipc.Cache);
		GetPrivateProfileString(Guid,L"Version",L"",Buf,ARRAYSIZE(Buf),ipc.Cache);
		wchar_t *p=Buf;
		for (int n=1,x=1,Number=0; p&&*p; p++)
		{
			if (*p==L'.') { n++; x=1; continue; }
			switch(n)
			{
				case 1: if (x) { ipc.Modules[i].Undo.Version.Major=StringToNumber(p,Number); x=0; } break;
				case 2: if (x) { ipc.Modules[i].Undo.Version.Minor=StringToNumber(p,Number); x=0; } break;
				case 3: if (x) { ipc.Modules[i].Undo.Version.Revision=StringToNumber(p,Number); x=0; } break;
				case 4: if (x) { ipc.Modules[i].Undo.Version.Build=StringToNumber(p,Number); x=0; } break;
			}
		}
		GetPrivateProfileString(Guid,L"MinFarVersion",L"",Buf,ARRAYSIZE(Buf),ipc.Cache);
		p=Buf;
		for (int n=1,x=1,Number=0; p&&*p; p++)
		{
			if (*p==L'.') { n++; x=1; continue; }
			switch(n)
			{
				case 1: if (x) { ipc.Modules[i].Undo.MinFarVersion.Major=StringToNumber(p,Number); x=0; } break;
				case 2: if (x) { ipc.Modules[i].Undo.MinFarVersion.Minor=StringToNumber(p,Number); x=0; } break;
				case 3: if (x) { ipc.Modules[i].Undo.MinFarVersion.Build=StringToNumber(p,Number); x=0; } break;
			}
		}
		if (ipc.Modules[i].Undo.MinFarVersion.Major && ipc.Modules[i].Undo.ArcName[0] && ipc.Modules[i].Undo.Date[0] &&
			(ipc.Modules[i].Undo.Version.Major || ipc.Modules[i].Undo.Version.Minor || ipc.Modules[i].Undo.Version.Revision || ipc.Modules[i].Undo.Version.Build))
			ipc.Modules[i].Flags|=UNDO;
	}
}

bool WriteCache(ModuleInfo *Cur)
{
	wchar_t Guid[37], Ver[64], FarVer[64];
	GuidToStr(Cur->Guid,Guid);
	wsprintf(Ver,L"%d.%d.%d.%d",Cur->New.Version.Major,Cur->New.Version.Minor,Cur->New.Version.Revision,Cur->New.Version.Build);
	wsprintf(FarVer,L"%d.%d.%d",Cur->New.MinFarVersion.Major,Cur->New.MinFarVersion.Minor,Cur->New.MinFarVersion.Build);

	if (WritePrivateProfileString(Guid,L"ArcName",Cur->New.ArcName,ipc.Cache) &&
			WritePrivateProfileString(Guid,L"Date",Cur->New.Date,ipc.Cache) &&
			WritePrivateProfileString(Guid,L"Version",Ver,ipc.Cache) &&
			WritePrivateProfileString(Guid,L"MinFarVersion",FarVer,ipc.Cache))
		return true;
	return false;
}
