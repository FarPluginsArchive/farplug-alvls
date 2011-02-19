/*
    PicView Advanced plugin for FAR Manager
    Copyright (C) 2003-2010 FARMail Group

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include "libgfl.h"
#include "farcolor.hpp"
#include "plugin.hpp"
#include "farkeys.hpp"
//#include "crt.hpp"

struct PluginStartupInfo Info;
struct FarStandardFunctions FSF;
wchar_t PluginRootKey[32768];
struct Options
{
  int AutomaticInViewer;
  int AutomaticInQuickView;
  int BilinearResizeInViewer;
  int BilinearResizeInQuickView;
  int Override;
  int IgnoreReadError;
  int ShowInViewer;
} Opt;

wchar_t **Exts=NULL;
DWORD ExtsNum=0;

enum
{
  MTitle,
  MAutomaticInViewer,
  MAutomaticInQuickView,
  MBilinearResizeInViewer,
  MBilinearResizeInQuickView,
  MOverride,
  MIgnoreReadError,
  MShowInViewer,
  MImageInfo,
};

const wchar_t *GetMsg(int MsgId)
{
  return(Info.GetMsg(Info.ModuleNumber,MsgId));
}

HKEY CreateRegKey(void)
{
  HKEY hKey;
  DWORD Disposition;
  RegCreateKeyEx(HKEY_CURRENT_USER,PluginRootKey,0,NULL,0,KEY_WRITE,NULL,&hKey,&Disposition);
  return(hKey);
}

HKEY OpenRegKey(void)
{
  HKEY hKey;
  if (RegOpenKeyEx(HKEY_CURRENT_USER,PluginRootKey,0,KEY_QUERY_VALUE,&hKey)!=ERROR_SUCCESS)
    return(NULL);
  return(hKey);
}

void SetRegKey(const wchar_t *ValueName,DWORD ValueData)
{
  HKEY hKey=CreateRegKey();
  RegSetValueEx(hKey,ValueName,0,REG_DWORD,(BYTE *)&ValueData,sizeof(ValueData));
  RegCloseKey(hKey);
}

int GetRegKey(const wchar_t *ValueName,int *ValueData,DWORD Default)
{
  HKEY hKey=OpenRegKey();
  DWORD Type,Size=sizeof(*ValueData);
  int ExitCode=RegQueryValueEx(hKey,ValueName,0,&Type,(BYTE *)ValueData,&Size);
  RegCloseKey(hKey);
  if (hKey==NULL || ExitCode!=ERROR_SUCCESS)
  {
    *ValueData=Default;
    return(FALSE);
  }
  return(TRUE);
}

void GetDIBFromBitmap(GFL_BITMAP *bitmap,BITMAPINFOHEADER *bitmap_info,unsigned char **data)
{
  int bytes_per_line;

  *data=NULL;
  memset(bitmap_info,0,sizeof(BITMAPINFOHEADER));

  bitmap_info->biSize=sizeof(BITMAPINFOHEADER);
  bitmap_info->biWidth=bitmap->Width;
  bitmap_info->biHeight=bitmap->Height;
  bitmap_info->biPlanes=1;

  bytes_per_line=(bitmap->Width*3+3)&-4;
  bitmap_info->biClrUsed=0;
  bitmap_info->biBitCount=24;
  bitmap_info->biCompression=BI_RGB;
  bitmap_info->biSizeImage=bytes_per_line*bitmap->Height;
  bitmap_info->biClrImportant=0;

  *data=(unsigned char*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,bitmap_info->biSizeImage);

  if(*data)
    memcpy(*data,bitmap->Data,bitmap_info->biSizeImage);

  return;
}

RECT RangingPic(RECT DCRect,GFL_BITMAP *RawPicture)
{
  float asp_dst=(float)(DCRect.right-DCRect.left)/(float)(DCRect.bottom-DCRect.top);
  float asp_src=(float)RawPicture->Width/(float)RawPicture->Height;

  int dst_w;
  int dst_h;

  if(asp_dst<asp_src)
  {
    dst_w=min(DCRect.right-DCRect.left,RawPicture->Width);
    dst_h=(int)(dst_w/asp_src);
  }
  else
  {
    dst_h=min(DCRect.bottom-DCRect.top,RawPicture->Height);
    dst_w=(int)(asp_src*dst_h);
  }

  RECT dest={DCRect.left,DCRect.top,dst_w,dst_h};
  return dest;
}

bool CheckName(const wchar_t *AFileName)
{
  int flen=lstrlen(AFileName);
  for(DWORD i=0;i<ExtsNum&&Exts;i++)
    if(Exts[i]&&lstrlen(Exts[i])<flen)
      if(!FSF.LStricmp(AFileName+flen-lstrlen(Exts[i]),Exts[i])&&AFileName[flen-lstrlen(Exts[i])-1]==L'.') return true;
  return false;
}

enum
{
  LEFT,
  RIGHT,
  CENTER
};

enum
{
  QUICKVIEW,
  VIEWER
};

struct DialogData
{
  HWND FarWindow;
  RECT DrawRect;
  RECT GDIRect;
  wchar_t FileName[32768];
  bool Redraw;
  bool SelfKeys;
  bool CurPanel;
  bool Loaded;
  bool FirstRun;
  long ResKey;
  BITMAPINFOHEADER *BmpHeader;
  unsigned char *DibData;
  GFL_FILE_INFORMATION *pic_info;
  int Align;
  int ShowingIn;
  int Page;
  int Rotate;
};

// {66ffe4cf-ee00-4b45-b91a-163f7c57d084}
static const GUID DlgGUID={0x66ffe4cf,0xee00,0x4b45,{0xb9,0x1a,0x16,0x3f,0x7c,0x57,0xd0,0x84}};

bool DrawImage(DialogData *data)
{
  bool result=false;
  GFL_BITMAP *RawPicture=NULL;
  data->DibData=NULL;

  RECT rect;
  CONSOLE_SCREEN_BUFFER_INFO info;
  GetClientRect(data->FarWindow,&rect);
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&info);

  int dx=rect.right/(info.srWindow.Right-info.srWindow.Left);
  int dy=rect.bottom/(info.srWindow.Bottom-info.srWindow.Top);

  RECT DCRect;
  DCRect.left=dx*(data->DrawRect.left-info.srWindow.Left);
  DCRect.right=dx*(data->DrawRect.right+1-info.srWindow.Left);
  DCRect.top=dy*(data->DrawRect.top/*-info.srWindow.Top*/);          //костыль для запуска far.exe /w
  DCRect.bottom=dy*(data->DrawRect.bottom+1/*-info.srWindow.Top*/);  //костыль для запуска far.exe /w

  RECT RangedRect;
  {
    GFL_LOAD_PARAMS load_params;
    gflGetDefaultLoadParams(&load_params);
    load_params.Flags|=GFL_LOAD_SKIP_ALPHA;
    // обнаружилось, что новая библиотека gfl 3.11 некоторые файлы не хочет грузить, говорит- GFL_ERROR:2
    // старая АНСИ gfl 2.20 грузит эти же файлы без проблем
    // выставим GFL_LOAD_IGNORE_READ_ERROR :)
    if(Opt.IgnoreReadError)
      load_params.Flags|=GFL_LOAD_IGNORE_READ_ERROR;
    load_params.Origin=GFL_BOTTOM_LEFT;
    load_params.LinePadding=4;
    load_params.ImageWanted=data->Page-1;
    GFL_ERROR res=gflLoadBitmapW(data->FileName,&RawPicture,&load_params,data->pic_info);
    if(res)
    {
/*
      wchar_t buf[80]; FSF.itoa(res, buf,10);
      wchar_t *MsgItems[4];
      MsgItems[0]=(wchar_t *)GetMsg(MTitle);
      MsgItems[1]=L"gflLoadBitmapW()";
      MsgItems[2]=L"GFL_ERROR:";
      MsgItems[3]=buf;
      Info.Message(Info.ModuleNumber,FMSG_WARNING|FMSG_MB_OK,0,MsgItems,sizeof(MsgItems)/sizeof(MsgItems[0]),1);
*/
      RawPicture=NULL;
    }
    if(RawPicture)
    {
      if(!gflChangeColorDepth(RawPicture,NULL,GFL_MODE_TO_BGR,GFL_MODE_NO_DITHER) && !gflRotate(RawPicture,NULL,data->Rotate,0))
      {
        GFL_BITMAP *pic=NULL;
        RangedRect=RangingPic(DCRect,RawPicture);
        if(data->Align==RIGHT)
        {
          RangedRect.left=DCRect.right-RangedRect.right;
        }
        else if(data->Align==CENTER)
        {
          RangedRect.left+=(DCRect.right-DCRect.left-RangedRect.right)/2;
          RangedRect.top+=(DCRect.bottom-DCRect.top-RangedRect.bottom)/2;
        }
        gflResize(RawPicture,&pic,RangedRect.right,RangedRect.bottom, 
                    (data->ShowingIn==VIEWER?(Opt.BilinearResizeInViewer?GFL_RESIZE_BILINEAR:0):(Opt.BilinearResizeInQuickView?GFL_RESIZE_BILINEAR:0)),0);
        if(pic)
        {
          GetDIBFromBitmap(pic,data->BmpHeader,&data->DibData);
          gflFreeBitmap(pic);
        }
      }
    }
  }
  if(RawPicture&&data->DibData)
  {
    result=true;
    data->GDIRect=RangedRect;
  }
  if(RawPicture)
    gflFreeBitmap(RawPicture);
  return result;
}

bool UpdateImage(DialogData *data, bool CheckOnly=false)
{
  if(!data->DibData&&!data->Loaded)
  {
    if(DrawImage(data))
    {
      data->Loaded=true;
      if ((!(data->FirstRun))&&(!CheckOnly))
        InvalidateRect(data->FarWindow,NULL,TRUE);
    }
  }
  if(!data->DibData||!data->Loaded)
    return false;
  if(CheckOnly)
    return true;
  HDC hDC=GetDC(data->FarWindow);
  StretchDIBits(hDC,data->GDIRect.left,data->GDIRect.top,data->GDIRect.right,data->GDIRect.bottom,0,0,data->GDIRect.right,data->GDIRect.bottom,data->DibData,(BITMAPINFO *)data->BmpHeader,DIB_RGB_COLORS,SRCCOPY);
  ReleaseDC(data->FarWindow,hDC);
  return true;
}

void FreeImage(DialogData *data)
{
  if(data->DibData)
  {
    HeapFree(GetProcessHeap(),0,data->DibData);
    data->DibData=NULL;
  }
  gflFreeFileInformation(data->pic_info);
}

void UpdateInfoText(HANDLE hDlg, DialogData *data)
{
  wchar_t string[512];
  wchar_t *types[]={L"RGB",L"GREY",L"CMY",L"CMYK",L"YCBCR",L"YUV16",L"LAB",L"LOGLUV",L"LOGL"};
  FSF.sprintf(string,GetMsg(MImageInfo),data->pic_info->Width,data->pic_info->Height,data->GDIRect.right,data->GDIRect.bottom,data->pic_info->Xdpi,data->pic_info->Ydpi,data->Page,data->pic_info->NumberOfImages,types[data->pic_info->ColorModel]);
  Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,2,(LONG_PTR)string);
  COORD coord = {0,0};
  Info.SendDlgMessage(hDlg,DM_SETCURSORPOS,2,(LONG_PTR)&coord);
}

LONG_PTR WINAPI PicDialogProc(HANDLE hDlg,int Msg,int Param1,LONG_PTR Param2)
{
  DialogData *DlgParams=(DialogData *)Info.SendDlgMessage(hDlg,DM_GETDLGDATA,0,0);

  switch(Msg)
  {
    case DN_INITDIALOG:
      Info.SendDlgMessage(hDlg,DM_SETDLGDATA,0,Param2);
      break;
    case DN_CTLCOLORDLGITEM:
      if(Param1==0)
      {
        if(DlgParams->ShowingIn==VIEWER)
          return (Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_VIEWERSTATUS)<<24)|(Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_VIEWERSTATUS)<<16)|(Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_VIEWERTEXT)<<8)|(Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_VIEWERSTATUS));
        else
          return (Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_PANELTEXT)<<16)|(Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)(DlgParams->SelfKeys?COL_PANELSELECTEDTITLE:COL_PANELTITLE))<<8)|(Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)(DlgParams->SelfKeys||DlgParams->CurPanel?COL_PANELSELECTEDTITLE:COL_PANELTITLE)));
      }
      if(Param1==2)
      {
        DlgParams->Redraw=true;
        if(DlgParams->ShowingIn==VIEWER)
          return (Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_VIEWERSTATUS)<<24)|(Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_VIEWERSTATUS)<<16)|(Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_VIEWERTEXT)<<8)|(Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_VIEWERSTATUS));
        else
          return (Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_PANELTEXT)<<24)|(Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_PANELTEXT)<<16)|(Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_PANELCURSOR)<<8)|(Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_PANELTEXT));
      }
      break;
    case DN_DRAWDLGITEM:
      DlgParams->Redraw=true;
      break;
    case DN_ENTERIDLE:
      if(DlgParams->Redraw)
      {
        DlgParams->Redraw=false;
        UpdateImage(DlgParams);
        if(DlgParams->FirstRun)
        {
          DlgParams->FirstRun=false;
          UpdateInfoText(hDlg,DlgParams);
        }
      }
      break;
    case 0x3FFF:
      if(Param1)
        UpdateImage(DlgParams);
      break;
    case DN_GOTFOCUS:
      if(DlgParams->SelfKeys)
        Info.SendDlgMessage(hDlg,DM_SETFOCUS,2,0);
      break;
    case DN_GETDIALOGINFO:
      if(((DialogInfo*)(Param2))->StructSize != sizeof(DialogInfo))
        return FALSE;
      ((DialogInfo*)(Param2))->Id=DlgGUID;
      return TRUE;
    case DN_KEY:
      if(!DlgParams->SelfKeys)
      {
        if((Param2&(KEY_CTRL|KEY_ALT|KEY_SHIFT|KEY_RCTRL|KEY_RALT))==Param2) break;
        switch(Param2)
        {
          case KEY_CTRLR:
            UpdateImage(DlgParams);
            return TRUE;
          case KEY_CTRLD:
          case KEY_CTRLS:
          case KEY_CTRLE:
          {
            FreeImage(DlgParams);
            if(Param2==KEY_CTRLD) DlgParams->Rotate-=90;
            else if (Param2==KEY_CTRLS) DlgParams->Rotate+=90;
            else DlgParams->Rotate+=180;
            DlgParams->Loaded=false;
            UpdateImage(DlgParams);
            UpdateInfoText(hDlg,DlgParams);
            return TRUE;
          }
          case KEY_TAB:
            DlgParams->SelfKeys=true;
            Info.SendDlgMessage(hDlg,DM_SETFOCUS,2,0);
            return TRUE;
          case KEY_BS:
          case KEY_SPACE:
            if(DlgParams->ShowingIn==VIEWER)
              Param2=Param2==KEY_BS?KEY_SUBTRACT:KEY_ADD;
          default:
            if(DlgParams->ShowingIn==VIEWER && Param2==KEY_F3)
              Param2=KEY_ESC;
            if(DlgParams->ShowingIn==QUICKVIEW && Param2==KEY_DEL)
              Param2=KEY_F8;
            DlgParams->ResKey=Param2;
            Info.SendDlgMessage(hDlg,DM_CLOSE,-1,0);
            return TRUE;
        }
      }
      else
      {
        switch(Param2)
        {
          case KEY_TAB:
            DlgParams->SelfKeys=false;
            Info.SendDlgMessage(hDlg,DM_SETFOCUS,1,0);
            return TRUE;
          case KEY_ADD:
          case KEY_SUBTRACT:
            if(DlgParams->DibData)
            {
              int Pages=DlgParams->pic_info->NumberOfImages;
              FreeImage(DlgParams);
              DlgParams->Loaded=false;
              if(Param2==KEY_ADD) DlgParams->Page++;
              else DlgParams->Page--;
              if(DlgParams->Page<1) DlgParams->Page=Pages;
              if(DlgParams->Page>Pages) DlgParams->Page=1;
              UpdateImage(DlgParams);
              UpdateInfoText(hDlg,DlgParams);
            }
            return TRUE;
        }
      }
      break;
  }
  return Info.DefDlgProc(hDlg,Msg,Param1,Param2);
}

void GetJiggyWithIt(HANDLE XPanelInfo,bool Override, bool Force)
{
  ViewerInfo info;
  info.StructSize=sizeof(info);
  if(Info.ViewerControl(VCTL_GETINFO,&info))
  {
    DialogData data;
    PanelInfo PInfo;
    Info.Control(XPanelInfo,FCTL_GETPANELINFO,0,(LONG_PTR)&PInfo);

    if(info.WindowSizeX==(PInfo.PanelRect.right-PInfo.PanelRect.left-1)&&PInfo.PanelType==PTYPE_QVIEWPANEL)
    {
      if(!Opt.AutomaticInQuickView && !Force)
        return;
      data.ShowingIn=QUICKVIEW;
    }
    else
    {
      if(!Opt.AutomaticInViewer && !Force)
        return;
      data.ShowingIn=VIEWER;
    }
    if (PInfo.Focus)
      data.CurPanel=true;
    else
      data.CurPanel=false;
    if(CheckName(info.FileName)||Override)
    {
      RECT ViewerRect;
      if(data.ShowingIn==VIEWER)
      {
        ViewerRect.left=0;
        ViewerRect.top=0;
        ViewerRect.right=info.WindowSizeX-1;
        ViewerRect.bottom=info.WindowSizeY+1;
      }
      data.FarWindow=(HWND)Info.AdvControl(Info.ModuleNumber,ACTL_GETFARHWND,0);
      size_t Size=FSF.ConvertPath(CPM_NATIVE,info.FileName,NULL,0);
      FSF.ConvertPath(CPM_NATIVE,info.FileName,data.FileName,Size>=32768?32767:Size);

      if(data.ShowingIn == VIEWER)
      {
        data.DrawRect=ViewerRect;
        data.DrawRect.top++;
        data.DrawRect.bottom--;
        data.Align=CENTER;
      }
      else
      {
        ViewerRect=data.DrawRect=PInfo.PanelRect;
        data.DrawRect.left++;
        data.DrawRect.top++;
        data.DrawRect.right--;
        data.DrawRect.bottom-=2;
        data.Align=CENTER;//(PInfo.PanelRect.left>0)?RIGHT:LEFT;
      }
      data.FirstRun=true;
      data.Redraw=false;
      data.SelfKeys=false;
      data.Loaded=false;
      data.ResKey=0;
      BITMAPINFOHEADER BmpHeader;
      data.BmpHeader=&BmpHeader;
      data.DibData=NULL;
      GFL_FILE_INFORMATION pic_info;
      data.pic_info=&pic_info;
      data.Page=1;
      data.Rotate=0;

      //HANDLE hs=Info.SaveScreen(0,0,-1,-1);
      if(UpdateImage(&data,true))
      {
        FarDialogItem DialogItems[3];
        memset(DialogItems,0,sizeof(DialogItems));
        unsigned int VBufSize; int color;
        if(data.ShowingIn==VIEWER)
        {
          DialogItems[0].Type=DI_EDIT;
          DialogItems[0].X1=0; DialogItems[0].X2=info.WindowSizeX-1;
          DialogItems[0].Y1=0; DialogItems[0].Y2=0;
          DialogItems[0].PtrData=info.FileName;
          DialogItems[1].Type=DI_USERCONTROL; DialogItems[1].Focus=TRUE;
          DialogItems[1].X1=0; DialogItems[1].X2=info.WindowSizeX-1;
          DialogItems[1].Y1=1; DialogItems[1].Y2=info.WindowSizeY;
          DialogItems[2].Type=DI_EDIT;
          DialogItems[2].X1=0; DialogItems[2].X2=info.WindowSizeX-1;
          DialogItems[2].Y1=info.WindowSizeY+1;
          DialogItems[2].Flags=DIF_READONLY;
          VBufSize=(info.WindowSizeY)*(info.WindowSizeX);
          color=Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_VIEWERTEXT);
        }
        else
        {
          DialogItems[0].Type=DI_DOUBLEBOX;
          DialogItems[0].X1=0; DialogItems[0].X2=PInfo.PanelRect.right-PInfo.PanelRect.left;
          DialogItems[0].Y1=0; DialogItems[0].Y2=PInfo.PanelRect.bottom-PInfo.PanelRect.top;
          DialogItems[0].PtrData=FSF.PointToName(info.FileName);
          DialogItems[1].Type=DI_USERCONTROL; DialogItems[1].Focus=TRUE;
          DialogItems[1].X1=1; DialogItems[1].X2=PInfo.PanelRect.right-PInfo.PanelRect.left-1;
          DialogItems[1].Y1=1; DialogItems[1].Y2=PInfo.PanelRect.bottom-PInfo.PanelRect.top-2;
          DialogItems[2].Type=DI_EDIT;
          DialogItems[2].X1=1; DialogItems[2].X2=PInfo.PanelRect.right-PInfo.PanelRect.left-1;
          DialogItems[2].Y1=PInfo.PanelRect.bottom-PInfo.PanelRect.top-1;
          DialogItems[2].Flags=DIF_READONLY;
          VBufSize=(PInfo.PanelRect.right-PInfo.PanelRect.left-1)*(PInfo.PanelRect.bottom-PInfo.PanelRect.top-2);
          color=Info.AdvControl(Info.ModuleNumber,ACTL_GETCOLOR,(void*)COL_PANELTEXT);
        }

        color=color&0xF0;
        color=color|(color>>4);

        CHAR_INFO *VirtualBuffer;
        VirtualBuffer=(CHAR_INFO *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,VBufSize*sizeof(CHAR_INFO));

        if (VirtualBuffer)
        {
          DialogItems[1].VBuf=VirtualBuffer;
          for(unsigned int i=0;i<VBufSize;i++)
          {
            VirtualBuffer[i].Char.UnicodeChar=L'.';
            VirtualBuffer[i].Attributes=color;
          }

          HANDLE hDlg=Info.DialogInit(Info.ModuleNumber,ViewerRect.left,ViewerRect.top,ViewerRect.right,ViewerRect.bottom,NULL,DialogItems,sizeof(DialogItems)/sizeof(DialogItems[0]),0,FDLG_SMALLDIALOG|FDLG_NODRAWSHADOW,PicDialogProc,(LONG_PTR)&data);
          if (hDlg != INVALID_HANDLE_VALUE)
          {
            Info.DialogRun(hDlg);
            Info.DialogFree(hDlg);
          }

          HeapFree(GetProcessHeap(),0,VirtualBuffer);
        }

        FreeImage(&data);

        if(data.ResKey)
        {
          KeySequence key;
          key.Flags=KSFLAGS_DISABLEOUTPUT;
          DWORD ResKey[]={KEY_CTRL|KEY_F10,KEY_ESC};
          if (data.ResKey==KEY_ESC && data.ShowingIn==VIEWER)
          {
            key.Count=2;
            key.Sequence=ResKey;
          }
          else
          { 
            key.Count=1;
            key.Sequence=(DWORD *)&data.ResKey;
          }
          Info.AdvControl(Info.ModuleNumber,ACTL_POSTKEYSEQUENCE,&key);
        }
      }
      else
      {
        //Info.RestoreScreen(NULL);
      }
      //Info.RestoreScreen(hs);
    }
  }
}

void SetDefaultExtentions()
{
  int number=gflGetNumberOfFormat();
  GFL_FORMAT_INFORMATION finfo;
  for(int i=0;i<number;i++)
  {
    gflGetFormatInformationByIndex(i,&finfo);
    for(DWORD j=0;j<finfo.NumberOfExtension;j++)
    {
      Exts=(wchar_t **)(((Exts)?HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,Exts,(ExtsNum+1)*sizeof(*Exts)):HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(ExtsNum+1)*sizeof(*Exts))));
      int size = MultiByteToWideChar(CP_ACP,0,finfo.Extension[j],-1,0,0);
      Exts[ExtsNum]=(wchar_t *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size*sizeof(wchar_t));
      MultiByteToWideChar(CP_ACP,0,finfo.Extension[j],-1,Exts[ExtsNum],size-1);
      ExtsNum++;
    }
  }
}

int WINAPI _export ProcessViewerEventW(int Event,void *Param)
{
  if(Event==VE_READ)
  {
    HANDLE XPanelInfo=PANEL_PASSIVE;
    struct WindowInfo wi;
    wi.Pos=-1;
    Info.AdvControl(Info.ModuleNumber,ACTL_GETSHORTWINDOWINFO,(void *)&wi);
    if (wi.Type==WTYPE_PANELS)
    {
      Info.Control(PANEL_PASSIVE,FCTL_REDRAWPANEL,0,0);
      Info.Control(PANEL_ACTIVE,FCTL_REDRAWPANEL,0,0);

      struct PanelInfo pi;
      Info.Control(PANEL_ACTIVE,FCTL_GETPANELINFO,0,(LONG_PTR)&pi);
      if (pi.PanelType==PTYPE_QVIEWPANEL)
        XPanelInfo=PANEL_ACTIVE;
    }
    GetJiggyWithIt(XPanelInfo,Opt.Override?true:false,false);
  }
  return 0;
}

HANDLE WINAPI _export OpenPluginW(int OpenFrom, INT_PTR Item)
{
  GetJiggyWithIt(PANEL_ACTIVE,true,true);
  return INVALID_HANDLE_VALUE;
}

int WINAPI _export GetMinFarVersionW() { return MAKEFARVERSION(2,0,1666); }

void WINAPI _export SetStartupInfoW(const struct PluginStartupInfo *Info)
{
  ::Info=*Info;
  if (Info->StructSize >= (int)sizeof(struct PluginStartupInfo))
  {
    FSF = *Info->FSF;
    ::Info.FSF = &FSF;
    FSF.sprintf(PluginRootKey,L"%s\\PicViewAdv",::Info.RootKey);
    wchar_t folder[32768];
    lstrcpy(folder, Info->ModuleName);
    (wchar_t)*(FSF.PointToName(folder)) = 0;
    gflSetPluginsPathnameW(folder);
    gflLibraryInit();
    gflEnableLZW(GFL_TRUE);
    SetDefaultExtentions();
    GetRegKey(L"AutomaticInViewer",&Opt.AutomaticInViewer,0);
    GetRegKey(L"AutomaticInQuickView",&Opt.AutomaticInQuickView,1);
    GetRegKey(L"BilinearResizeInViewer",&Opt.BilinearResizeInViewer,1);
    GetRegKey(L"BilinearResizeInQuickView",&Opt.BilinearResizeInQuickView,0);
    GetRegKey(L"Override",&Opt.Override,0);
    GetRegKey(L"IgnoreReadError",&Opt.IgnoreReadError,1);
    GetRegKey(L"ShowInViewer",&Opt.ShowInViewer,1);
  }
}

void WINAPI _export GetPluginInfoW(struct PluginInfo *Info)
{
  static const wchar_t *MenuStrings[1];
  Info->StructSize=sizeof(*Info);
  Info->Flags=PF_DISABLEPANELS|PF_VIEWER;
  MenuStrings[0]=GetMsg(MTitle);
  Info->PluginMenuStrings=MenuStrings;
  Info->PluginMenuStringsNumber=Opt.ShowInViewer?1:0;
  Info->PluginConfigStrings=MenuStrings;
  Info->PluginConfigStringsNumber=1;
}

void WINAPI _export ExitFARW()
{
  gflLibraryExit();
  if(Exts)
  {
    for(DWORD i=0;i<ExtsNum;i++)
      if(Exts[i])
        HeapFree(GetProcessHeap(),0,Exts[i]);
    HeapFree(GetProcessHeap(),0,Exts);
  }
  Exts=NULL;
  ExtsNum=0;
}

int WINAPI _export ConfigureW(int ItemNumber)
{
  (void)ItemNumber;
  FarDialogItem DialogItems[8];

  memset(DialogItems,0,sizeof(DialogItems));
  DialogItems[0].Type=DI_DOUBLEBOX;
  DialogItems[0].X1=3; DialogItems[0].X2=50;
  DialogItems[0].Y1=1; DialogItems[0].Y2=9;
  DialogItems[0].PtrData=GetMsg(MTitle);

  DialogItems[1].Type=DI_CHECKBOX;
  DialogItems[1].X1=5;
  DialogItems[1].Y1=2;
  DialogItems[1].Focus=TRUE;
  GetRegKey(L"AutomaticInViewer",&Opt.AutomaticInViewer,0);
  DialogItems[1].Selected=Opt.AutomaticInViewer;
  DialogItems[1].DefaultButton = 1;
  DialogItems[1].PtrData=GetMsg(MAutomaticInViewer);

  DialogItems[2].Type=DI_CHECKBOX;
  DialogItems[2].X1=5;
  DialogItems[2].Y1=3;
  GetRegKey(L"AutomaticInQuickView",&Opt.AutomaticInQuickView,1);
  DialogItems[2].Selected=Opt.AutomaticInQuickView;
  DialogItems[2].PtrData=GetMsg(MAutomaticInQuickView);

  DialogItems[3].Type=DI_CHECKBOX;
  DialogItems[3].X1=5;
  DialogItems[3].Y1=4;
  GetRegKey(L"BilinearResizeInViewer",&Opt.BilinearResizeInViewer,1);
  DialogItems[3].Selected=Opt.BilinearResizeInViewer;
  DialogItems[3].PtrData=GetMsg(MBilinearResizeInViewer);

  DialogItems[4].Type=DI_CHECKBOX;
  DialogItems[4].X1=5;
  DialogItems[4].Y1=5;
  GetRegKey(L"BilinearResizeInQuickView",&Opt.BilinearResizeInQuickView,0);
  DialogItems[4].Selected=Opt.BilinearResizeInQuickView;
  DialogItems[4].PtrData=GetMsg(MBilinearResizeInQuickView);

  DialogItems[5].Type=DI_CHECKBOX;
  DialogItems[5].X1=5;
  DialogItems[5].Y1=6;
  GetRegKey(L"Override",&Opt.Override,0);
  DialogItems[5].Selected=Opt.Override;
  DialogItems[5].PtrData=GetMsg(MOverride);

  DialogItems[6].Type=DI_CHECKBOX;
  DialogItems[6].X1=5;
  DialogItems[6].Y1=7;
  GetRegKey(L"IgnoreReadError",&Opt.IgnoreReadError,1);
  DialogItems[6].Selected=Opt.IgnoreReadError;
  DialogItems[6].PtrData=GetMsg(MIgnoreReadError);

  DialogItems[7].Type=DI_CHECKBOX;
  DialogItems[7].X1=5;
  DialogItems[7].Y1=8;
  GetRegKey(L"ShowInViewer",&Opt.ShowInViewer,1);
  DialogItems[7].Selected=Opt.ShowInViewer;
  DialogItems[7].PtrData=GetMsg(MShowInViewer);

  HANDLE hDlg=Info.DialogInit(Info.ModuleNumber,-1,-1,53,11,NULL,DialogItems,sizeof(DialogItems)/sizeof(DialogItems[0]),0,0,0,0);

  if (hDlg != INVALID_HANDLE_VALUE)
  {
    if (Info.DialogRun(hDlg) != -1)
    {
      Opt.AutomaticInViewer=Info.SendDlgMessage(hDlg,DM_GETCHECK,1,0);
      Opt.AutomaticInQuickView=Info.SendDlgMessage(hDlg,DM_GETCHECK,2,0);
      Opt.BilinearResizeInViewer=Info.SendDlgMessage(hDlg,DM_GETCHECK,3,0);
      Opt.BilinearResizeInQuickView=Info.SendDlgMessage(hDlg,DM_GETCHECK,4,0);
      Opt.Override=Info.SendDlgMessage(hDlg,DM_GETCHECK,5,0);
      Opt.IgnoreReadError=Info.SendDlgMessage(hDlg,DM_GETCHECK,6,0);
      Opt.ShowInViewer=Info.SendDlgMessage(hDlg,DM_GETCHECK,7,0);
      SetRegKey(L"AutomaticInViewer",Opt.AutomaticInViewer);
      SetRegKey(L"AutomaticInQuickView",Opt.AutomaticInQuickView);
      SetRegKey(L"BilinearResizeInViewer",Opt.BilinearResizeInViewer);
      SetRegKey(L"BilinearResizeInQuickView",Opt.BilinearResizeInQuickView);
      SetRegKey(L"Override",Opt.Override);
      SetRegKey(L"IgnoreReadError",Opt.IgnoreReadError);
      SetRegKey(L"ShowInViewer",Opt.ShowInViewer);
      Info.DialogFree(hDlg);
      return TRUE;
    }
    Info.DialogFree(hDlg);
  }
  return FALSE;
}

#if defined(__GNUC__)

#ifdef __cplusplus
extern "C"{
#endif
  bool WINAPI DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved);
#ifdef __cplusplus
};
#endif

bool WINAPI DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved)
{
  (void)hDll;
  (void)dwReason;
  (void)lpReserved;
  return true;
}

#endif
