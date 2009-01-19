/****************************************************************************
 * VisRen6_DLG.cpp
 *
 * Plugin module for FAR Manager 1.71
 *
 * Copyright (c) 2007 Alexey Samlyukov
 ****************************************************************************/

/****************************************************************************
 **************************** ShowDialog functions **************************
 ****************************************************************************/


/****************************************************************************
 * ID-��������� �������
 ****************************************************************************/
enum {
  DlgBORDER = 0,    // 0
  DlgLMASKNAME,     // 1
  DlgEMASKNAME,     // 2
  DlgLTEMPL,        // 3
  DlgETEMPLNAME,    // 4
  DlgBTEMPLNAME,    // 5

  DlgLMASKEXT,      // 6
  DlgEMASKEXT,      // 7
  DlgETEMPLEXT,     // 8
  DlgBTEMPLEXT,     // 9

  DlgSEP1,          //10
  DlgLSEARCH,       //11
  DlgESEARCH,       //12
  DlgLREPLACE,      //13
  DlgEREPLACE,      //14
  DlgCASE,          //15
  DlgREGEX,         //16

  DlgSEP2,          //17
  DlgLIST,          //18
  DlgSEP3_LOG,      //19
  DlgREN,           //20
  DlgUNDO,          //21
  DlgEDIT,          //22
  DlgCANCEL         //23
};


/****************************************************************************
 * ������ �������.
 ****************************************************************************/
static struct DlgSize {
  // ��������� �������
  bool Full;
  // ����������
  DWORD W;
  DWORD W2;
  DWORD WS;
  DWORD H;
  // �����������������
  DWORD mW;
  DWORD mW2;
  DWORD mWS;
  DWORD mH;
} DlgSize;


/****************************************************************************
 * ������� ��� �������������� ������� �������� InitDialogItem � FarDialogItem.
 ****************************************************************************/
static void InitDialogItems(const InitDialogItem *Init, FarDialogItem *Item, int ItemsNumber)
{
  while(ItemsNumber--)
  {
    Item->Type           = Init->Type;
    Item->X1             = Init->X1;
    Item->Y1             = Init->Y1;
    Item->X2             = Init->X2;
    Item->Y2             = Init->Y2;
    Item->Focus          = Init->Focus;
    Item->Param.Selected = Init->Selected;
    Item->Flags          = Init->Flags;
    Item->DefaultButton  = Init->DefaultButton;
    lstrcpy( Item->Data.Data, ((unsigned int)Init->Data < 2000) ?
             GetMsg((unsigned int)Init->Data) : Init->Data );

    Item++;
    Init++;
  }
}


/***************************************************************************
 * ������ � ��������� ������� ��� �������
 ***************************************************************************/
static void GetDlgSize()
{
  DlgSize.W=DlgSize.mW=80-2;
  DlgSize.WS=DlgSize.mWS=(80-2)-37;
  DlgSize.W2=DlgSize.mW2=(80-2)/2-2;
  DlgSize.H=DlgSize.mH=25-2;

  HANDLE hConOut = CreateFile(_T("CONOUT$"), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  CONSOLE_SCREEN_BUFFER_INFO csbiNfo;
  if (GetConsoleScreenBufferInfo(hConOut, &csbiNfo))
  {
    if (csbiNfo.dwSize.X-2 > DlgSize.mW)
    {
      DlgSize.mW=(csbiNfo.dwSize.X-2>132 ? 132 : csbiNfo.dwSize.X-2);
      DlgSize.mWS=DlgSize.mW-37;
      DlgSize.mW2=(DlgSize.mW)/2-2;
    }
    if (csbiNfo.dwSize.Y-2 > DlgSize.mH) DlgSize.mH=csbiNfo.dwSize.Y-2;
  }
  CloseHandle(hConOut);
}

/****************************************************************************
 * ������ ������������ ����� ������, ����� ����� � ����� ����������
 ****************************************************************************/
static void LenItems(const TCHAR *FileName, bool bDest)
{
  const TCHAR *start=FileName;
  while (*FileName++)
    ;
  const TCHAR *end=FileName-1;

  if (bDest)
    Opt.lenDestFileName=max(Opt.lenDestFileName, end-start);
  else
    Opt.lenFileName=max(Opt.lenFileName, end-start);

  while (--FileName != start && *FileName != _T('.'))
    ;
  if (*FileName == _T('.'))
    (bDest?Opt.lenDestExt:Opt.lenExt)=max((bDest?Opt.lenDestExt:Opt.lenExt), end-FileName-1);
  if (FileName != start)
    (bDest?Opt.lenDestName:Opt.lenName)=max((bDest?Opt.lenDestName:Opt.lenName), FileName-start);
}

/***************************************************************************
 * ���������/���������� ����� ������ � �������
 ***************************************************************************/
static bool UpdateFarList(HANDLE hDlg, bool bFull, bool bUndoList)
{
  Opt.lenFileName=Opt.lenName=Opt.lenExt=0;
  Opt.lenDestFileName=Opt.lenDestName=Opt.lenDestExt=0;
  int widthSrc, widthDest;
  widthSrc=widthDest=(bFull?DlgSize.mW2:DlgSize.W2)-2;
  if (Opt.CurBorder!=0) { widthSrc+=Opt.CurBorder; widthDest-=Opt.CurBorder; }
  FarList List;
  List.ItemsNumber=(bUndoList?sUndoFI.iCount:sFI.iCount);
  // ��� ����������� ������ ������� ����� ���������
  int AddNumber=((bFull?DlgSize.mH:DlgSize.H)-4)-9+1;
  if (List.ItemsNumber<AddNumber) AddNumber-=List.ItemsNumber;
  else AddNumber=0;

  FarListItem *ListItems=(FarListItem *)my_malloc((List.ItemsNumber+AddNumber)*sizeof(FarListItem));
  if (ListItems)
  {
    // ����������� ��������� �������
    FarListPos ListPos;
    Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
    static bool bOldFull=bFull;
    if (ListPos.SelectPos>=List.ItemsNumber) ListPos.SelectPos=List.ItemsNumber?List.ItemsNumber-1:0;
    if (bOldFull!=bFull) { ListPos.TopPos=-1; bOldFull=bFull; }

    for (int i=0; i<List.ItemsNumber; i++)
    {
      TCHAR *src =(bUndoList?sUndoFI.CurFileName[i]:sFI.ppi[i].FindData.cFileName),
            *dest=(bUndoList?sUndoFI.OldFileName[i]:sFI.DestFileName[i]);
      LenItems(src, 0); LenItems(dest, 1);

      int lenSrc=lstrlen(src), posSrc=Opt.srcCurCol;
      if (lenSrc<=widthSrc) posSrc=0;
      else if (posSrc>lenSrc-widthSrc) posSrc=lenSrc-widthSrc;

      int lenDest=lstrlen(dest), posDest=Opt.destCurCol;
      if (lenDest<=widthDest) posDest=0;
      else if (posDest>lenDest-widthDest) posDest=lenDest-widthDest;

      FSF.sprintf( ListItems[i].Text, _T("%-*.*s%c%-*.*s"), widthSrc, widthSrc, src+posSrc,
                   0x000000B3, widthDest, widthDest, (bError?GetMsg(MError):dest+posDest) );
    }

    for (int i=List.ItemsNumber; i<List.ItemsNumber+AddNumber; i++)
    {
      FSF.sprintf( ListItems[i].Text, _T("%-*.*s%c%-*.*s"), widthSrc, widthSrc, _T(""),
                   0x000000B3, widthDest, widthDest, _T("") );
    }

    List.ItemsNumber+=AddNumber;
    List.Items=ListItems;
    Info.SendDlgMessage(hDlg, DM_LISTSET, DlgLIST, (LONG_PTR)&List);
    my_free(ListItems);
    Info.SendDlgMessage(hDlg, DM_LISTSETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
    Info.SendDlgMessage(hDlg, DM_LISTSETMOUSEREACTION, DlgLIST, (LONG_PTR)LMRT_NEVER);
    return true;
  }
  return false;
}

/***************************************************************************
 * ��������� ������� �������
 ***************************************************************************/
static void DlgResize(HANDLE hDlg, bool bF5=false)
{
  COORD c;
  if (bF5) // ������ F5
  {
    if (DlgSize.Full==0)  // ��� ���������� ������
    {
      c.X=DlgSize.mW; c.Y=DlgSize.mH;
      DlgSize.Full=true;  // ���������� ������������
    }
    else
    {
      c.X=DlgSize.W; c.Y=DlgSize.H;
      DlgSize.Full=false; // ������� ����������
    }
  }
  else // ����� ������ ����������� ������� �������
  {
    if (DlgSize.Full) { c.X=DlgSize.mW; c.Y=DlgSize.mH; }
    else { c.X=DlgSize.W; c.Y=DlgSize.H; }
  }
  Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
  Opt.srcCurCol=Opt.destCurCol=Opt.CurBorder=0;
  FarDialogItem Item;
  for (int i=DlgBORDER; i<=DlgCANCEL; i++)
  {
    Info.SendDlgMessage(hDlg, DM_GETDLGITEM, i, (LONG_PTR)&Item);
    switch (i)
    {
      case DlgBORDER:
        Item.X2=(DlgSize.Full?DlgSize.mW:DlgSize.W)-1;
        Item.Y2=(DlgSize.Full?DlgSize.mH:DlgSize.H)-1;
        break;
      case DlgEMASKNAME:
        Item.X2=(DlgSize.Full?DlgSize.mWS:DlgSize.WS)-3;
        break;
      case DlgLMASKEXT:
      case DlgCASE:
      case DlgREGEX:
        Item.X1=(DlgSize.Full?DlgSize.mWS:DlgSize.WS);
        break;
      case DlgEMASKEXT:
        Item.X1=(DlgSize.Full?DlgSize.mWS:DlgSize.WS);
        Item.X2=(DlgSize.Full?DlgSize.mW:DlgSize.W)-3;
        break;
      case DlgETEMPLEXT:
        Item.X1=(DlgSize.Full?DlgSize.mWS:DlgSize.WS);
        Item.X2=(DlgSize.Full?DlgSize.mW:DlgSize.W)-8;
        break;
      case DlgBTEMPLEXT:
        Item.X1=(DlgSize.Full?DlgSize.mW:DlgSize.W)-5;
        break;
      case DlgESEARCH:
      case DlgEREPLACE:
        Item.X2=(DlgSize.Full?DlgSize.mWS:DlgSize.WS)-3;
        break;
      case DlgLIST:
      {
        Item.X2=(DlgSize.Full?DlgSize.mW:DlgSize.W)-2;
        Item.Y2=(DlgSize.Full?DlgSize.mH:DlgSize.H)-4;

        // !!! ������� ����������� FARa �� �������� ListItems �� FarDialogItem
        UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo);
        break;
      }
      case DlgSEP3_LOG:
        Item.Y1=(DlgSize.Full?DlgSize.mH:DlgSize.H)-3;
        break;
      case DlgREN:
      case DlgUNDO:
      case DlgEDIT:
      case DlgCANCEL:
        Item.Y1=(DlgSize.Full?DlgSize.mH:DlgSize.H)-2;
        break;
    }
    Info.SendDlgMessage(hDlg, DM_SETDLGITEM, i, (LONG_PTR)&Item);
  }
  Info.SendDlgMessage(hDlg, DM_RESIZEDIALOG, 0, (LONG_PTR)&c);
  c.X=c.Y=-1;
  Info.SendDlgMessage(hDlg, DM_MOVEDIALOG, true, (LONG_PTR)&c);
  Info.SendDlgMessage(hDlg, DM_LISTSETMOUSEREACTION, DlgLIST, (LONG_PTR)LMRT_NEVER);
  Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
  return;
}

/****************************************************************************
 * ������������ ������ ����� �� ���������� �������
 ****************************************************************************/
static bool SetMask(HANDLE hDlg, DWORD dwMask, DWORD dwTempl)
{
  TCHAR buf[512], buf2[512], templ[10];
  Info.SendDlgMessage(hDlg, DM_GETTEXTPTR, dwMask, (LONG_PTR)buf);
  int length=lstrlen(buf);
  COORD Pos;
  Info.SendDlgMessage(hDlg, DM_GETCURSORPOS, dwMask, (LONG_PTR)&Pos);
  EditorSelect es;
  Info.SendDlgMessage(hDlg, DM_GETSELECTION, dwMask, (LONG_PTR)&es);
  FarListPos ListPos;
  Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, dwTempl, (LONG_PTR)&ListPos);
  if (dwTempl==DlgETEMPLNAME)
  {
    switch (ListPos.SelectPos)
    {
      case 0:  lstrcpy(templ, _T("[N]"));      break;
      case 1:  FSF.sprintf(templ, _T("[N1-%d]"), Opt.lenName); break;
      case 2:  lstrcpy(templ, _T("[C1+1]"));   break;
      //---
      case 4:  lstrcpy(templ, _T("[L]"));      break;
      case 5:  lstrcpy(templ, _T("[U]"));      break;
      case 6:  lstrcpy(templ, _T("[F]"));      break;
      case 7:  lstrcpy(templ, _T("[T]"));      break;
      //---
      case 9:  lstrcpy(templ, _T("[#]"));      break;
      case 10: lstrcpy(templ, _T("[t]"));      break;
      case 11: lstrcpy(templ, _T("[a]"));      break;
      case 12: lstrcpy(templ, _T("[l]"));      break;
      case 13: lstrcpy(templ, _T("[y]"));      break;
      case 14: lstrcpy(templ, _T("[g]"));      break;
      //---
      case 16: lstrcpy(templ, _T("[c]"));     break;
      case 17: lstrcpy(templ, _T("[m]"));     break;
      case 18: lstrcpy(templ, _T("[d]"));     break;
      case 19: lstrcpy(templ, _T("[r]"));     break;
      //---
      case 21: lstrcpy(templ, _T("[DM]"));     break;
      case 22: lstrcpy(templ, _T("[TM]"));     break;
      case 23: lstrcpy(templ, _T("[TL]"));     break;
      case 24: lstrcpy(templ, _T("[TR]"));     break;
    }
  }
  else
  {
    switch (ListPos.SelectPos)
    {
      case 0:  lstrcpy(templ, _T("[E]"));      break;
      case 1:  FSF.sprintf(templ, _T("[E1-%d]"), Opt.lenExt); break;
      case 2:  lstrcpy(templ, _T("[C1+1]")); break;
      //---
      case 4:  lstrcpy(templ, _T("[L]"));      break;
      case 5:  lstrcpy(templ, _T("[U]"));      break;
      case 6:  lstrcpy(templ, _T("[F]"));      break;
      case 7:  lstrcpy(templ, _T("[T]"));      break;
    }
  }
  if (lstrlen(templ)+length>=512) return false;

  if (es.BlockType!=BTYPE_NONE && es.BlockStartPos>=0)
  {
    Pos.X=es.BlockStartPos;
    lstrcpy(buf2, buf+es.BlockStartPos+es.BlockWidth);
    buf[Pos.X]=_T('\0'); lstrcat(buf,buf2);
  }
  if (Pos.X>=length)
    lstrcat(buf, templ);
  else
  {
    lstrcpy(buf2, buf+Pos.X); buf[Pos.X]=_T('\0');
    lstrcat(buf, templ); lstrcat(buf, buf2);
  }
  FarDialogItem Item;
  Info.SendDlgMessage(hDlg, DM_SETTEXTPTR, dwMask, (LONG_PTR)FSF.Trim(buf));
  Info.SendDlgMessage(hDlg, DM_SETFOCUS, dwMask, 0);
  Info.SendDlgMessage(hDlg, DM_GETDLGITEM, dwMask, (LONG_PTR)&Item);
  Info.SendDlgMessage(hDlg, DN_EDITCHANGE, dwMask, (LONG_PTR)&Item);
  Pos.X+=lstrlen(templ);
  Info.SendDlgMessage(hDlg, DM_SETCURSORPOS, dwMask, (LONG_PTR)&Pos);
  es.BlockType=BTYPE_NONE;
  Info.SendDlgMessage(hDlg, DM_SETSELECTION, dwMask, (LONG_PTR)&es);
  return true;
}


/****************************************************************************
 * ��������� ��������� ����� � ������� �����
 ****************************************************************************/
static void MouseSelect(HANDLE hDlg, DWORD dwStr, DWORD dwMousePosX)
{
  SMALL_RECT dlgRect, itemRect;
  Info.SendDlgMessage(hDlg, DM_GETDLGRECT, 0, (LONG_PTR)&dlgRect);
  Info.SendDlgMessage(hDlg, DM_GETITEMPOSITION, dwStr, (LONG_PTR)&itemRect);
  EditorSetPosition esp;
  Info.SendDlgMessage(hDlg, DM_GETEDITPOSITION, dwStr, (LONG_PTR)&esp);
  int length=Info.SendDlgMessage(hDlg, DM_GETTEXTLENGTH, dwStr, 0);
  int CurPos=dwMousePosX-(dlgRect.Left+itemRect.Left);

  if (dwMousePosX<=(dlgRect.Left+itemRect.Left) && esp.LeftPos>0) esp.LeftPos-=1;
  else if (dwMousePosX>=(dlgRect.Left+itemRect.Right) && CurPos+esp.LeftPos<length) esp.LeftPos+=1;

  if (CurPos+esp.LeftPos<0) CurPos=0;
  else if (CurPos+esp.LeftPos>length) CurPos=length;
  else CurPos+=esp.LeftPos;

  esp.CurPos=esp.CurTabPos=CurPos;

  if (bStartSelect)
  {
    StartPosX=CurPos; bStartSelect=false;
  }

  EditorSelect es; es.BlockType=BTYPE_COLUMN; es.BlockStartLine=es.BlockHeight=0;
  if (CurPos>StartPosX)
  {
    es.BlockStartPos=StartPosX; es.BlockWidth=CurPos-StartPosX;
  }
  else
  {
    es.BlockStartPos=CurPos; es.BlockWidth=StartPosX-CurPos;
  }

  Info.SendDlgMessage(hDlg, DM_SETSELECTION, dwStr, (LONG_PTR)&es);
  Info.SendDlgMessage(hDlg, DM_SETEDITPOSITION, dwStr, (LONG_PTR)&esp);
  return;
}

/****************************************************************************
 * ��������� ����� ������ �������� ���� � ����� ������
 ****************************************************************************/
static int ListMouseRightClick(HANDLE hDlg, DWORD dwMousePosX, DWORD dwMousePosY)
{
  SMALL_RECT dlgRect;
  Info.SendDlgMessage(hDlg, DM_GETDLGRECT, 0, (LONG_PTR)&dlgRect);
  FarListPos ListPos;
  Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
  int Pos=ListPos.TopPos+(dwMousePosY-(dlgRect.Top+9));
  if ( Pos>=(Opt.LoadUndo?sUndoFI.iCount:sFI.iCount)
       // �������� �� ��������� ������ DlgLIST
       || dwMousePosX<=dlgRect.Left  || dwMousePosX>=dlgRect.Right
       || dwMousePosY<=dlgRect.Top+8 || dwMousePosY>=dlgRect.Bottom-2
     )
    return -1;
  ListPos.SelectPos=Pos;
  Info.SendDlgMessage(hDlg, DM_LISTSETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
  return Pos;
}

/****************************************************************************
 * �-��� ���������� ������� ��� �� ����� ������ �� ��������� �����
 ****************************************************************************/
static void ShowName(int Pos)
{
  int i;
  TCHAR srcName[4][66], destName[4][66];

  for (i=0; i<4; i++)
  {
    srcName[i][0]=_T('\0'); destName[i][0]=_T('\0');
  }
  // ������ ���
  TCHAR *src=Opt.LoadUndo?sUndoFI.CurFileName[Pos]:sFI.ppi[Pos].FindData.cFileName;
  int len=lstrlen(src);
  for (i=0; i<4; i++, len-=65)
  {
    if (len<65)
    {
      lstrcpyn(srcName[i], src+i*65, len+1);
      break;
    }
    else
      lstrcpyn(srcName[i], src+i*65, 66);
  }
  // ����� ���
  TCHAR *dest=Opt.LoadUndo?sUndoFI.OldFileName[Pos]:sFI.DestFileName[Pos];
  len=lstrlen(dest);
  for (i=0; i<4; i++, len-=65)
  {
    if (len<65)
    {
      lstrcpyn(destName[i], dest+i*65, len+1);
      break;
    }
    else
      lstrcpyn(destName[i], dest+i*65, 66);
  }

  FarDialogItem DialogItems[15];
  memset(DialogItems, 0, sizeof(DialogItems));
  DialogItems[0].Type=DI_DOUBLEBOX; lstrcpy(DialogItems[0].Data.Data, GetMsg(MFullFileName));
  DialogItems[0].X1=0; DialogItems[0].Y1=0; DialogItems[0].X2=70; DialogItems[0].Y2=14;
  DialogItems[1].Type=DI_SINGLEBOX;
  lstrcpy(DialogItems[1].Data.Data, GetMsg(MOldName)); DialogItems[1].Flags=DIF_LEFTTEXT;
  DialogItems[1].X1=2; DialogItems[1].Y1=1; DialogItems[1].X2=68; DialogItems[1].Y2=6;
  DialogItems[7].Type=DI_SINGLEBOX;
  lstrcpy(DialogItems[7].Data.Data, GetMsg(MNewName)); DialogItems[7].Flags=DIF_LEFTTEXT;
  DialogItems[7].X1=2; DialogItems[7].Y1=7; DialogItems[7].X2=68; DialogItems[7].Y2=12;
  for(i=2; i<6; i++)
  {
    DialogItems[i].Type=DI_TEXT;
    DialogItems[i].X1=3; DialogItems[i].Y1=i; lstrcpy(DialogItems[i].Data.Data, srcName[i-2]);
  }
  for(i=8; i<11; i++)
  {
    DialogItems[i].Type=DI_TEXT;
    DialogItems[i].X1=3; DialogItems[i].Y1=i; lstrcpy(DialogItems[i].Data.Data, destName[i-8]);
  }
  DialogItems[13].Type=DI_BUTTON; lstrcpy(DialogItems[13].Data.Data, GetMsg(MOK));
  DialogItems[13].Y1=13; DialogItems[13].Flags=DIF_CENTERGROUP;

  Info.DialogEx(Info.ModuleNumber,-1,-1,71,15,0,
                DialogItems,sizeof(DialogItems) / sizeof(DialogItems[0]),0,FDLG_SMALLDIALOG,0,0);
  return;
}

/****************************************************************************
 * ���������� ������� ��� ShowDialog
 ****************************************************************************/
static LONG_PTR WINAPI ShowDialogProc(HANDLE hDlg, int Msg, int Param1, LONG_PTR Param2)
{
  switch (Msg)
  {
    case DN_INITDIALOG:
      {
        Opt.Search[0]=_T('\0');
        Opt.Replace[0]=_T('\0');
        lstrcpy(Opt.WordDiv, _T("-. _"));
        if (HKEY hKey=CreateOrOpenRegKey(false, PluginRootKey))
        {
          TCHAR cpRegValue[NM];
          if (GetRegKey(hKey, _T("WordDiv"), cpRegValue, sizeof(cpRegValue)))
            lstrcpy(Opt.WordDiv, cpRegValue);
          RegCloseKey(hKey);
        }
        Opt.CurBorder=Opt.srcCurCol=Opt.destCurCol=0;
        Opt.CaseSensitive=Opt.LogRen=1;
        Opt.RegEx=Opt.LoadUndo=Opt.Undo=0;
        StartPosX=Focus=-1;
        bStartSelect=true;
        bListFocus=bError=false;

        Info.SendDlgMessage(hDlg, DM_SETCOMBOBOXEVENT, DlgETEMPLNAME, (LONG_PTR)CBET_KEY);
        Info.SendDlgMessage(hDlg, DM_SETCOMBOBOXEVENT, DlgETEMPLEXT, (LONG_PTR)CBET_KEY);

        if (!Opt.UseLastHistory)
        {
          Info.SendDlgMessage(hDlg, DM_SETTEXTPTR, DlgEMASKNAME, (LONG_PTR)_T("[N]"));
          Info.SendDlgMessage(hDlg, DM_SETTEXTPTR, DlgEMASKEXT, (LONG_PTR)_T("[E]"));
        }
        // ��������� ���������� ������ �������
        DlgResize(hDlg);
        // ��� ����������� ������������� ����� �� �������
        FarDialogItem Item;
        Info.SendDlgMessage(hDlg, DM_GETDLGITEM, DlgEMASKNAME, (LONG_PTR)&Item);
        Info.SendDlgMessage(hDlg, DN_EDITCHANGE, DlgEMASKNAME, (LONG_PTR)&Item);
        Info.SendDlgMessage(hDlg, DM_GETDLGITEM, DlgEMASKEXT, (LONG_PTR)&Item);
        Info.SendDlgMessage(hDlg, DN_EDITCHANGE, DlgEMASKEXT, (LONG_PTR)&Item);
        break;
      }

  /************************************************************************/

    case DN_RESIZECONSOLE:
      GetDlgSize();
      DlgResize(hDlg);
      return true;

  /************************************************************************/

    case DN_DRAWDLGITEM:
      switch (Param1)
      {
        case DlgEMASKNAME: case DlgETEMPLNAME: case DlgBTEMPLNAME:
        case DlgEMASKEXT:  case DlgETEMPLEXT:  case DlgBTEMPLEXT:
        case DlgESEARCH:   case DlgEREPLACE:   case DlgCASE:
        case DlgREGEX:
        case DlgEDIT:
          if (Opt.LoadUndo || !sFI.iCount) ((FarDialogItem*)Param2)->Flags|=DIF_DISABLE;
          else ((FarDialogItem*)Param2)->Flags&=~DIF_DISABLE;
          return true;
        case DlgREN:
        {
          if (!sFI.iCount || bError) ((FarDialogItem*)Param2)->Flags|=DIF_DISABLE;
          else ((FarDialogItem*)Param2)->Flags&=~DIF_DISABLE;
          return true;
        }
        case DlgUNDO:
        {
          if (!sUndoFI.iCount || Opt.LoadUndo || bError) ((FarDialogItem*)Param2)->Flags|=DIF_DISABLE;
          else ((FarDialogItem*)Param2)->Flags&=~DIF_DISABLE;
          return true;
        }
        case DlgSEP2:
        {
          TCHAR sep[MAX_PATH];
          FSF.TruncStr(lstrcpy(sep, sUndoFI.Dir), (DlgSize.Full?DlgSize.mW-6:DlgSize.W-6));
          FSF.sprintf(((FarDialogItem*)Param2)->Data.Data, _T(" %s "), (Opt.LoadUndo?sep:GetMsg(MSep)));
          return true;
        }
        case DlgSEP3_LOG:
        {
          TCHAR sep[80];
          lstrcpy(sep, Opt.LogRen?GetMsg(MCreateLog):GetMsg(MNoCreateLog));
          lstrcat(sep, sUndoFI.iCount? _T("* ") : _T(" "));
          lstrcpy(((FarDialogItem*)Param2)->Data.Data, sep);
          return true;
        }
      }
      break;

  /************************************************************************/

    case DN_BTNCLICK:
      if (Param1==DlgBTEMPLNAME)
      {
        if (!SetMask(hDlg, DlgEMASKNAME, DlgETEMPLNAME)) return false;
      }
      else if (Param1==DlgBTEMPLEXT)
      {
        if (!SetMask(hDlg, DlgEMASKEXT, DlgETEMPLEXT)) return false;
      }
      else if (Param1==DlgCASE || Param1==DlgREGEX)
      {
        Param1==DlgCASE?Opt.CaseSensitive=Param2:Opt.RegEx=Param2;
        FarDialogItem Item;
        Info.SendDlgMessage(hDlg, DM_GETDLGITEM, DlgESEARCH, (LONG_PTR)&Item);
        Info.SendDlgMessage(hDlg, DN_EDITCHANGE, DlgESEARCH, (LONG_PTR)&Item);
      }
      else if (Param1==DlgUNDO)
      {
        Opt.srcCurCol=Opt.destCurCol=Opt.CurBorder=0;
        UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo=1);
        Info.SendDlgMessage(hDlg, DM_SETFOCUS, DlgREN, 0);
      }
      break;

  /************************************************************************/

    case DN_MOUSECLICK:
      if (Param1==DlgBORDER && ((MOUSE_EVENT_RECORD *)Param2)->dwEventFlags==DOUBLE_CLICK)
        goto DLGRESIZE;
      else if (Param1==DlgSEP3_LOG &&
              ((MOUSE_EVENT_RECORD *)Param2)->dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED)
        goto LOGREN;
      else if (Param1==DlgSEP3_LOG &&
              ((MOUSE_EVENT_RECORD *)Param2)->dwButtonState==RIGHTMOST_BUTTON_PRESSED)
        goto CLEARLOGREN;
      else if (Param1==DlgLIST &&
              ((MOUSE_EVENT_RECORD *)Param2)->dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED &&
              ((MOUSE_EVENT_RECORD *)Param2)->dwEventFlags==DOUBLE_CLICK )
        goto GOTOFILE;
      else if (Param1==DlgLIST &&
              ((MOUSE_EVENT_RECORD *)Param2)->dwButtonState==RIGHTMOST_BUTTON_PRESSED)
      {
        Info.SendDlgMessage(hDlg, DM_SETFOCUS, DlgLIST, 0);
        goto RIGHTCLICK;
      }
      return false;

  /************************************************************************/

    case DN_GOTFOCUS:
      {
        bool Ret=true;
        switch (Param1)
        {
          case DlgEMASKNAME:
            Focus=DlgEMASKNAME; break;
          case DlgEMASKEXT:
            Focus=DlgEMASKEXT; break;
          case DlgESEARCH:
            Focus=DlgESEARCH; break;
          case DlgEREPLACE:
            Focus=DlgEREPLACE; break;
          case DlgLIST:
            Focus=DlgLIST; break;
          default:
            /* $ !!  skirda, 10.07.2007 1:27:03:
             *       � ����������� ���� ����� ����� ������ ���� ShowDialog(-1)
             *       � � ����������� ���������� - ShowDialog(OldPos); ShowDialog(FocusPos);
             *
             *       AS,  �.�. ��� ���� ��� ���� ������� "DN_DRAWDIALOG"
             *            � ��� ����� "DN_DRAWDLGITEM"
             *       ����� ���� ����� ��� DN_CTLCOLORDLGITEM:
             */
            Info.SendDlgMessage(hDlg, DM_SHOWITEM, DlgSEP2, 1);
             /* $ */
            Ret=false; break;
        }
        if (Ret) Info.SendDlgMessage(hDlg, DM_SETMOUSEEVENTNOTIFY, 1, 0);
      }
      break;

  /************************************************************************/

    case DN_MOUSEEVENT:
      {
 RIGHTCLICK:
        MOUSE_EVENT_RECORD *MRec=(MOUSE_EVENT_RECORD *)Param2;
        if ( MRec->dwButtonState==FROM_LEFT_1ST_BUTTON_PRESSED &&
             MRec->dwEventFlags==MOUSE_MOVED )
        {
          switch (Focus)
          {
            case DlgEMASKNAME:
              MouseSelect(hDlg, DlgEMASKNAME, MRec->dwMousePosition.X);
              return false;
            case DlgEMASKEXT:
              MouseSelect(hDlg, DlgEMASKEXT, MRec->dwMousePosition.X);
              return false;
            case DlgESEARCH:
              MouseSelect(hDlg, DlgESEARCH, MRec->dwMousePosition.X);
              return false;
            case DlgEREPLACE:
              MouseSelect(hDlg, DlgEREPLACE, MRec->dwMousePosition.X);
              return false;
          }
        }
        // ��������� ����� ������ �������� ���� � ����� ������
        else if (MRec->dwButtonState==RIGHTMOST_BUTTON_PRESSED && Focus==DlgLIST)
        {

          int Pos=ListMouseRightClick(hDlg, MRec->dwMousePosition.X, MRec->dwMousePosition.Y);
          if (Pos>=0)
          {
            ShowName(Pos);
            return false;
          }
        }
        bStartSelect=true;
        return true;
      }

  /************************************************************************/

    case DN_KILLFOCUS:
      {
        switch (Param1)
          case DlgEMASKNAME:
          case DlgEMASKEXT:
          case DlgESEARCH:
          case DlgEREPLACE:
          case DlgLIST:
            Focus=-1;
            Info.SendDlgMessage(hDlg, DM_SETMOUSEEVENTNOTIFY, 0, 0);
            break;
      }
      break;

  /************************************************************************/

    case DN_KEY:
      if (Param2==KEY_F1 && (Param1==DlgETEMPLNAME || Param1==DlgETEMPLEXT))
      {
        Info.ShowHelp(Info.ModuleName, 0, 0);
        return true;
      }
      else if (Param2==KEY_F2 && !Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0))
      {
 LOGREN:
        int ItemFocus=Info.SendDlgMessage(hDlg, DM_GETFOCUS, 0, 0);
        if (Opt.LogRen) Opt.LogRen=0;
        else Opt.LogRen=1;
        Info.SendDlgMessage(hDlg, DM_SHOWITEM, DlgSEP3_LOG, 1); //  ���
        Info.SendDlgMessage(hDlg, DM_SHOWITEM, ItemFocus, 1);   //      �����������
        return true;
      }
      //----
      else if (Param2==KEY_F3)
      {
        int Pos=Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, 0);
        if (Pos<(Opt.LoadUndo?sUndoFI.iCount:sFI.iCount)) ShowName(Pos);
        return true;
      }
      //----
      else if (Param2==KEY_F4)
      {
        if ( (Param1==DlgETEMPLNAME || Param1==DlgETEMPLEXT) &&
             Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0) )
        {
          if (  (Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgETEMPLNAME, 0)==7 ||
                 Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgETEMPLEXT, 0)==7)
              &&
                 Info.InputBox( GetMsg(MWordDivTitle), GetMsg(MWordDivBody),
                               _T("VisRenWordDiv"), Opt.WordDiv, Opt.WordDiv,
                               sizeof(Opt.WordDiv)-1, 0, FIB_BUTTONS )  )
              SetRegKey(PluginRootKey, _T(""), _T("WordDiv"), (TCHAR *)Opt.WordDiv);
        }
        else
          Info.SendDlgMessage(hDlg, DM_CLOSE, DlgEDIT, 0);
        return true;
      }
      //----
      else if (Param2==KEY_F5 && !Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0))
      {
 DLGRESIZE:
        DlgResize(hDlg, true);  //true - �.�. ������ F5
        return true;
      }
      //----
      else if (Param2==KEY_F6 && sUndoFI.iCount && !bError &&
               !Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0))
      {
        Opt.srcCurCol=Opt.destCurCol=Opt.CurBorder=0;
        UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo=1);
        Info.SendDlgMessage(hDlg, DM_REDRAW, 0, 0);
        Info.SendDlgMessage(hDlg, DM_SETFOCUS, DlgREN, 0);
        return true;
      }
      //----
      else if (Param2==KEY_F8 && !Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0))
      {
 CLEARLOGREN:
        if (sUndoFI.iCount && !Opt.LoadUndo && YesNoMsg(MClearLogTitle, MClearLogBody))
        {
          FreeUndo();
          int ItemFocus=Info.SendDlgMessage(hDlg, DM_GETFOCUS, 0, 0);
          Info.SendDlgMessage(hDlg, DM_SHOWITEM, DlgUNDO, 1);          //  ���
          Info.SendDlgMessage(hDlg, DM_SHOWITEM, DlgSEP3_LOG, 1);      //
          Info.SendDlgMessage(hDlg, DM_SHOWITEM, ItemFocus, 1);        //      �����������
          Info.SendDlgMessage(hDlg, DM_SETFOCUS, (ItemFocus!=DlgUNDO?ItemFocus:DlgREN), 0);
          return true;
        }
        return false;
      }
      //----
      else if (Param2==KEY_F12 && !Info.SendDlgMessage(hDlg, DM_GETDROPDOWNOPENED, 0, 0))
      {
        if (!bListFocus)
        {
          ItemFocus=Info.SendDlgMessage(hDlg, DM_GETFOCUS, 0, 0);
          Info.SendDlgMessage(hDlg, DM_SETFOCUS, DlgLIST, 0);
          bListFocus=true;
        }
        else
        {
          Info.SendDlgMessage(hDlg, DM_SETFOCUS, ItemFocus, 0);
          bListFocus=false;
        }
        return true;
      }
      //----
      else if (Param2==KEY_INS)
      {
        if (Param1==DlgETEMPLNAME || Param1==DlgBTEMPLNAME)
        {
          if (SetMask(hDlg, DlgEMASKNAME, DlgETEMPLNAME))
          {
            if (Param1==DlgETEMPLNAME)
              Info.SendDlgMessage(hDlg, DM_SETFOCUS, DlgETEMPLNAME, 0);
            return true;
          }
        }
        else if (Param1==DlgETEMPLEXT || Param1==DlgBTEMPLEXT)
        {
          if (SetMask(hDlg, DlgEMASKEXT, DlgETEMPLEXT))
          {
            if (Param1==DlgETEMPLEXT)
              Info.SendDlgMessage(hDlg, DM_SETFOCUS, DlgETEMPLEXT, 0);
            return true;
          }
        }
      }
      //----
      else if (Param2==KEY_DEL && Param1==DlgLIST && !Opt.LoadUndo)
      {
        int Pos=Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, 0);
        if (Pos>=sFI.iCount) return false;
        Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
        FarListPos ListPos={Pos, -1};
        FarListDelete ListDel={Pos, 1};
        Info.SendDlgMessage(hDlg, DM_LISTDELETE, DlgLIST, (LONG_PTR)&ListDel);
        do
        {
          sFI.ppi[Pos]=sFI.ppi[Pos+1];
          lstrcpy(sFI.DestFileName[Pos], sFI.DestFileName[Pos+1]);
          Pos++;
        } while(Pos<sFI.iCount-1);
        my_free(&sFI.ppi[Pos]); my_free(sFI.DestFileName[Pos]); sFI.DestFileName[Pos]=0;
        --sFI.iCount;
        Info.SendDlgMessage(hDlg, DM_LISTSETCURPOS, DlgLIST, (LONG_PTR)&ListPos);
        UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo);
        Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
        return true;
      }
      //----
      else if ( Param1==DlgLIST && !Opt.LoadUndo &&
               (Param2==(KEY_CTRL|KEY_UP) || Param2==(KEY_RCTRL|KEY_UP) ||
                Param2==(KEY_CTRL|KEY_DOWN) || Param2==(KEY_RCTRL|KEY_DOWN)) )
      {
        int Pos=Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, 0);
        if (Pos>=sFI.iCount) return false;
        bool bUp=(Param2==(KEY_CTRL|KEY_UP) || Param2==(KEY_RCTRL|KEY_UP));
        if (bUp?Pos>0:Pos<sFI.iCount-1)
        {
          Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
          PluginPanelItem TempSrcItem=sFI.ppi[bUp?Pos-1:Pos+1];
          sFI.ppi[bUp?Pos-1:Pos+1]=sFI.ppi[Pos];
          sFI.ppi[Pos]=TempSrcItem;
          TCHAR *TempDestItem=sFI.DestFileName[bUp?Pos-1:Pos+1];
          sFI.DestFileName[bUp?Pos-1:Pos+1]=sFI.DestFileName[Pos];
          sFI.DestFileName[Pos]=TempDestItem;

          FarListGetItem ListGet;
          ListGet.ItemIndex=Pos;
          Info.SendDlgMessage(hDlg, DM_LISTGETITEM, DlgLIST, (LONG_PTR)&ListGet);

          FarListDelete ListDel={Pos, 1};
          Info.SendDlgMessage(hDlg, DM_LISTDELETE, DlgLIST, (LONG_PTR)&ListDel);

          FarListInsert ListIns;
          ListIns.Index=bUp?Pos-1:Pos+1;
          ListIns.Item=ListGet.Item;
          Info.SendDlgMessage(hDlg, DM_LISTINSERT, DlgLIST, (LONG_PTR)&ListIns);
          Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
          return true;
        }
      }
      //----
      else if ( Param1==DlgLIST &&
               (Param2==(KEY_CTRL|KEY_LEFT) || Param2==(KEY_RCTRL|KEY_LEFT) ||
                Param2==(KEY_CTRL|KEY_RIGHT) || Param2==(KEY_RCTRL|KEY_RIGHT) ||
                Param2==(KEY_CTRL|KEY_NUMPAD5) || Param2==(KEY_RCTRL|KEY_NUMPAD5)) )
      {
        bool bLeft=(Param2==(KEY_CTRL|KEY_LEFT) || Param2==(KEY_RCTRL|KEY_LEFT));
        int maxBorder=(DlgSize.Full?DlgSize.mW2:DlgSize.W2)-2-5;
        bool Ret=false;
        if (Param2==(KEY_CTRL|KEY_NUMPAD5) || Param2==(KEY_RCTRL|KEY_NUMPAD5))
        {
          Opt.srcCurCol=Opt.destCurCol=Opt.CurBorder=0; Ret=true;
        }
        else if (bLeft?Opt.CurBorder>-maxBorder:Opt.CurBorder<maxBorder)
        {
          if (bLeft)
          {
            Opt.CurBorder-=1;
          }
          else
          {
            Opt.CurBorder+=1;
          }
          Ret=true;
        }
        if (Ret)
        {
          Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
          Ret=UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo);
          Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
          if (Ret) return true;
          else return false;
        }
      }
      //----
      else if ( Param1==DlgLIST &&
               (Param2==KEY_LEFT || Param2==KEY_RIGHT ||
                Param2==(KEY_ALT|KEY_LEFT) || Param2==(KEY_RALT|KEY_LEFT) ||
                Param2==(KEY_ALT|KEY_RIGHT) || Param2==(KEY_RALT|KEY_RIGHT)) )
      {
        bool Ret=false;
        if (Param2==KEY_LEFT || Param2==KEY_RIGHT)
        {
          int maxDestCol=Opt.lenDestFileName-((DlgSize.Full?DlgSize.mW2:DlgSize.W2)-2)+Opt.CurBorder;
          if (Opt.destCurCol>maxDestCol)
            if (maxDestCol>0) Opt.destCurCol=maxDestCol;
            else Opt.destCurCol=0;
          if (Param2==KEY_LEFT && Opt.destCurCol>0)
          {
            Opt.destCurCol-=1; Ret=true;
          }
          else if (Param2==KEY_RIGHT && Opt.destCurCol<maxDestCol)
          {
            Opt.destCurCol+=1; Ret=true;
          }
        }
        else
        {
          int maxSrcCol=Opt.lenFileName-((DlgSize.Full?DlgSize.mW2:DlgSize.W2)-2)-Opt.CurBorder;
          if (Opt.srcCurCol>maxSrcCol)
            if (maxSrcCol>0) Opt.srcCurCol=maxSrcCol;
            else Opt.srcCurCol=0;
          if ((Param2==(KEY_ALT|KEY_LEFT) || Param2==(KEY_RALT|KEY_LEFT)) && Opt.srcCurCol>0)
          {
            Opt.srcCurCol-=1; Ret=true;
          }
          else if ((Param2==(KEY_ALT|KEY_RIGHT) || Param2==(KEY_RALT|KEY_RIGHT)) && Opt.srcCurCol<maxSrcCol)
          {
            Opt.srcCurCol+=1; Ret=true;
          }
        }
        if (Ret)
        {
          Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
          Ret=UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo);
          Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
        }
        return true;
      }
      //----
      else if ( Param1==DlgLIST &&
               (Param2==(KEY_CTRL|KEY_PGUP) || Param2==(KEY_RCTRL|KEY_PGUP)) )
      {
 GOTOFILE:
        int Pos=Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, DlgLIST, 0);
        if (Pos>=(Opt.LoadUndo?sUndoFI.iCount:sFI.iCount)) return true;

        TCHAR Name[MAX_PATH];
        GetCurrentDirectory(sizeof(Name), Name);
        if (Opt.LoadUndo && FSF.LStricmp(Name, sUndoFI.Dir))
          Info.Control(INVALID_HANDLE_VALUE, FCTL_SETPANELDIR, (void *)sUndoFI.Dir);

        PanelInfo PInfo;
        Info.Control(INVALID_HANDLE_VALUE, FCTL_GETPANELINFO, &PInfo);
        PanelRedrawInfo RInfo;
        RInfo.CurrentItem=PInfo.CurrentItem;
        RInfo.TopPanelItem=PInfo.TopPanelItem;
        lstrcpy(Name, Opt.LoadUndo?sUndoFI.CurFileName[Pos]:sFI.ppi[Pos].FindData.cFileName);

        for (int i=0; i<PInfo.ItemsNumber; i++)
        {
          if (!FSF.LStricmp(Name, PInfo.PanelItems[i].FindData.cFileName))
          {
            RInfo.CurrentItem=i;
            break;
          }
        }
        Info.Control(INVALID_HANDLE_VALUE, FCTL_REDRAWPANEL, &RInfo);
        Info.SendDlgMessage(hDlg, DM_CLOSE, DlgCANCEL, 0);
        return true;
      }
      break;

  /************************************************************************/

    case DN_EDITCHANGE:
      if ( Param1==DlgEMASKNAME || Param1==DlgEMASKEXT ||
           Param1==DlgESEARCH || Param1==DlgEREPLACE )
      {
        if (Param1==DlgEMASKNAME)
          lstrcpy(Opt.MaskName, ((FarDialogItem *)Param2)->Data.Data);
        else if (Param1==DlgEMASKEXT)
          lstrcpy(Opt.MaskExt, ((FarDialogItem *)Param2)->Data.Data);
        else if (Param1==DlgESEARCH)
          lstrcpy(Opt.Search, ((FarDialogItem *)Param2)->Data.Data);
        else if (Param1==DlgEREPLACE)
          lstrcpy(Opt.Replace, ((FarDialogItem *)Param2)->Data.Data);

        if (ProcessFileName())
        {
          bError=false;
          Opt.destCurCol=0;
        }
        else bError=true;
        Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, false, 0);
        bool Ret=UpdateFarList(hDlg, DlgSize.Full, false);
        Info.SendDlgMessage(hDlg, DM_ENABLEREDRAW, true, 0);
        if (Ret) return true;
        else return false;
      }
      break;

  /************************************************************************/

    case DN_CTLCOLORDLGITEM:
      // !!! ��. ����������� � DN_GOTFOCUS.
      // ������ ��������� ��� ����������...
      if (Param1==DlgSEP2)
      {
        int color=Info.AdvControl(Info.ModuleNumber, ACTL_GETCOLOR, (void *)COL_DIALOGBOX);
        // ... ���� �������� � ������, �� ������ � ���������� ����
        if (DlgLIST==Info.SendDlgMessage(hDlg, DM_GETFOCUS, 0, 0))
        {
          Param2=Info.AdvControl(Info.ModuleNumber, ACTL_GETCOLOR, (void *)COL_DIALOGHIGHLIGHTBOXTITLE);
          Param2|=(color<<16);
        }
        // ... ����� - � ������� ����
        else
        {
          Param2=color;
          Param2|=(color<<16);
        }
        return Param2;
      }
      break;

  /************************************************************************/

    case DN_CLOSE:
      if (Param1==DlgREN && Opt.LoadUndo)
      {
        const TCHAR *MsgItems[]={ GetMsg(MUndoTitle), GetMsg(MUndoBody) };
        switch (Info.Message(Info.ModuleNumber,FMSG_DOWN|FMSG_WARNING|FMSG_MB_YESNOCANCEL,0,MsgItems,2,0))
        {
          case 0:
            Opt.Undo=1; return true;
          case 1:
            UpdateFarList(hDlg, DlgSize.Full, Opt.LoadUndo=Opt.Undo=0);
            Info.SendDlgMessage(hDlg, DM_REDRAW, 0, 0);
            return false;
          default: return false;
        }
      }
      break;
  }

  return Info.DefDlgProc(hDlg, Msg, Param1, Param2);
}


/****************************************************************************
 * ������ ��������� �� �������, ���������� ������ � ������� ��������������,
 * ��������� ��������� Opt, ��������� (���� ����) ����� ��������� � �������.
 ****************************************************************************/
static int ShowDialog()
{
  GetDlgSize();

  struct InitDialogItem InitItems[] = {
    /* 0*/{DI_DOUBLEBOX,0,          0,DlgSize.W-1,DlgSize.H-1, 0, 0,                       0, 0, (TCHAR *)MVRenTitle},

    /* 1*/{DI_TEXT,     2,          1,          0, 0, 0, 0,                                0, 0, (TCHAR *)MMaskName},
    /* 2*/{DI_EDIT,     2,          2,DlgSize.WS-3,0, 1, (int)_T("VisRenMaskName"), DIF_USELASTHISTORY|DIF_HISTORY, 0, _T("")},
    /* 3*/{DI_TEXT,     2,          3,          0, 0, 0, 0,                                0, 0, (TCHAR *)MTempl},
    /* 4*/{DI_COMBOBOX, 2,          4,         31, 0, 0, 0,DIF_LISTAUTOHIGHLIGHT|DIF_DROPDOWNLIST|DIF_LISTWRAPMODE, 0, (TCHAR *)MTempl_1},
    /* 5*/{DI_BUTTON,  34,          4,          0, 0, 0, 0,    DIF_NOBRACKETS|DIF_BTNNOCLOSE, 0, (TCHAR *)MSet},

    /* 6*/{DI_TEXT,     DlgSize.WS, 1,          0, 0, 0, 0,                                0, 0, (TCHAR *)MMaskExt},
    /* 7*/{DI_EDIT,     DlgSize.WS, 2,DlgSize.W-3, 0, 0, (int)_T("VisRenMaskExt"),  DIF_USELASTHISTORY|DIF_HISTORY, 0, _T("")},
    /* 8*/{DI_COMBOBOX, DlgSize.WS, 4,DlgSize.W-8, 0, 0, 0,DIF_LISTAUTOHIGHLIGHT|DIF_DROPDOWNLIST|DIF_LISTWRAPMODE, 0, (TCHAR *)MTempl2_1},
    /* 9*/{DI_BUTTON,   DlgSize.W-5,4,          0, 0, 0, 0,    DIF_NOBRACKETS|DIF_BTNNOCLOSE, 0, (TCHAR *)MSet},


    /*10*/{DI_TEXT,     0,          5,          0, 0, 0, 0,                    DIF_SEPARATOR, 0, _T("")},
    /*11*/{DI_TEXT,     2,          6,         14, 0, 0, 0,                                0, 0, (TCHAR *)MSearch},
    /*12*/{DI_EDIT,    15,          6,DlgSize.WS-3,0, 0, (int)_T("VisRenSearch"),  DIF_HISTORY, 0, _T("")},
    /*13*/{DI_TEXT,     2,          7,         14, 0, 0, 0,                                0, 0, (TCHAR *)MReplace},
    /*14*/{DI_EDIT,    15,          7,DlgSize.WS-3,0, 0, (int)_T("VisRenReplace"), DIF_HISTORY, 0, _T("")},
    /*15*/{DI_CHECKBOX, DlgSize.WS, 6,         19, 0, 0, 1,                                0, 0, (TCHAR *)MCase},
    /*16*/{DI_CHECKBOX, DlgSize.WS, 7,         19, 0, 0, 0,                                0, 0, (TCHAR *)MRegEx},

    /*17*/{DI_TEXT,     0,          8,          0, 0, 0, 0,                    DIF_SEPARATOR, 0, _T("")},
    /*18*/{DI_LISTBOX,  2,          9,DlgSize.W-2,DlgSize.H-4, 0, 0, DIF_LISTNOCLOSE|DIF_LISTNOBOX, 0, _T("")},
    /*19*/{DI_TEXT,     0,DlgSize.H-3,          0, 0, 0, 0,                    DIF_SEPARATOR, 0, _T("")},

    /*20*/{DI_BUTTON,   0,DlgSize.H-2,          0, 0, 0, 0,                  DIF_CENTERGROUP, 1, (TCHAR *)MRen},
    /*21*/{DI_BUTTON,   0,DlgSize.H-2,          0, 0, 0, 0,   DIF_BTNNOCLOSE|DIF_CENTERGROUP, 0, (TCHAR *)MUndo},
    /*22*/{DI_BUTTON,   0,DlgSize.H-2,          0, 0, 0, 0,                  DIF_CENTERGROUP, 0, (TCHAR *)MEdit},
    /*23*/{DI_BUTTON,   0,DlgSize.H-2,          0, 0, 0, 0,                  DIF_CENTERGROUP, 0, (TCHAR *)MCancel}
  };
  struct FarDialogItem DialogItems[sizeof(InitItems) / sizeof(InitItems[0])];
  memset(DialogItems, 0, sizeof(DialogItems));
  InitDialogItems(InitItems, DialogItems, sizeof(InitItems) / sizeof(InitItems[0]));

  // ��������������� ������ � ���������
  FarListItem itemTempl1[25];
  int n = sizeof(itemTempl1) / sizeof(itemTempl1[0]);
  for (int i = 0; i < n; i++)
  {
    itemTempl1[i].Flags = ((i==3 || i==8 || i==15 || i==20)?LIF_SEPARATOR:0);
    lstrcpy(itemTempl1[i].Text, GetMsg(MTempl_1+i));
  }
  FarList Templates1 = {n, itemTempl1};
  DialogItems[DlgETEMPLNAME].Param.ListItems = &Templates1;

  FarListItem itemTempl2[8];
  n = sizeof(itemTempl2) / sizeof(itemTempl2[0]);
  for (int i = 0; i < n; i++)
  {
    itemTempl2[i].Flags = (i==3?LIF_SEPARATOR:0);
    lstrcpy(itemTempl2[i].Text, GetMsg(MTempl2_1+i));
  }
  FarList Templates2 = {n, itemTempl2};
  DialogItems[DlgETEMPLEXT].Param.ListItems = &Templates2;


  int ExitCode = Info.DialogEx( Info.ModuleNumber,
                                -1, -1, DlgSize.W, DlgSize.H,
                                "Contents",
                                DialogItems,
                                sizeof(DialogItems) / sizeof(DialogItems[0]),
                                0, FDLG_SMALLDIALOG,
                                ShowDialogProc,
                                0 );

  switch (ExitCode)
  {
    case DlgREN:
      Opt.UseLastHistory=1;
      return 0;
    case DlgEDIT:
      return 2;
    default:
      return 3;
  }
}