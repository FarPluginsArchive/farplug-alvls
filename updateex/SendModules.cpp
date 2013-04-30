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
