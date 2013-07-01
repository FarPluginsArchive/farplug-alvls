#include "plugin.hpp"

static struct PluginStartupInfo Info;
BOOL IsOldFar;

#define GetMsg(MsgId) (Info.GetMsg(Info.ModuleNumber,MsgId))

enum {
  MSelJmp2Plugin,

  MJumpFirst,
  MJumpPrev,
  MJumpNext,
  MJumpLast
};

//------------------------------------------------------------------------------
int WINAPI _export GetMinFarVersion() { return MAKEFARVERSION(1,70,2087); }

void WINAPI _export SetStartupInfo(const struct PluginStartupInfo *PInfo)
{
  Info = *PInfo;
  IsOldFar = (PInfo->StructSize < sizeof(struct PluginStartupInfo));
}

//------------------------------------------------------------------------------

void WINAPI _export GetPluginInfo(struct PluginInfo *PInfo)
{
  if (IsOldFar) return;

  PInfo->StructSize=sizeof(*PInfo);
  static char *PluginMenuStrings[1];
  PluginMenuStrings[0]=(char*)GetMsg(MSelJmp2Plugin);
  PInfo->PluginMenuStrings=PluginMenuStrings;
  PInfo->PluginMenuStringsNumber=sizeof(PluginMenuStrings)/sizeof(PluginMenuStrings[0]);
}

//------------------------------------------------------------------------------

HANDLE WINAPI _export OpenPlugin(int OpenFrom, int Item)
{
  if (IsOldFar) return(INVALID_HANDLE_VALUE);

  struct PanelInfo PInfo;
  Info.Control(INVALID_HANDLE_VALUE,FCTL_GETPANELINFO,&PInfo);

  if (PInfo.PanelType != PTYPE_FILEPANEL) return(INVALID_HANDLE_VALUE);

  static struct FarMenuItem MenuItems[4];
    lstrcpy(MenuItems[0].Text,GetMsg(MJumpFirst));
    lstrcpy(MenuItems[1].Text,GetMsg(MJumpPrev));
    lstrcpy(MenuItems[2].Text,GetMsg(MJumpNext));
    lstrcpy(MenuItems[3].Text,GetMsg(MJumpLast));

  int ExitCode=Info.Menu(Info.ModuleNumber,-1,-1,0,0,GetMsg(MSelJmp2Plugin),0,0,0,0,MenuItems,4);

  if (PInfo.SelectedItemsNumber)
  {
    MessageBox(0,0,0,MB_OK);
    int i;
    struct PanelRedrawInfo RInfo={0,PInfo.TopPanelItem};

    switch(ExitCode)
    {
      case 0: // First
        for (i = 0; i < PInfo.ItemsNumber; i++)
        {
          if (PInfo.PanelItems[i].Flags & PPIF_SELECTED)
          {
            RInfo.CurrentItem = i;
            Info.Control(INVALID_HANDLE_VALUE,FCTL_REDRAWPANEL,&RInfo);
            break;
          }
        }
        break;

      case 1: // Prev
        i = PInfo.CurrentItem;
        while (--i >= 0)
        {
          if (PInfo.PanelItems[i].Flags & PPIF_SELECTED)
          {
            RInfo.CurrentItem = i;
            Info.Control(INVALID_HANDLE_VALUE,FCTL_REDRAWPANEL,&RInfo);
            break;
          }
        }
        break;

      case 2: // Next
        i = PInfo.CurrentItem;
        while (++i < PInfo.ItemsNumber)
        {
          if (PInfo.PanelItems[i].Flags & PPIF_SELECTED)
          {
            RInfo.CurrentItem = i;
            Info.Control(INVALID_HANDLE_VALUE,FCTL_REDRAWPANEL,&RInfo);
            break;
          }
        }
        break;

      case 3: // Last
        for (i = PInfo.ItemsNumber; i >= 0 ; i--)
        {
          if (PInfo.PanelItems[i].Flags & PPIF_SELECTED)
          {
            RInfo.CurrentItem = i;
            Info.Control(INVALID_HANDLE_VALUE,FCTL_REDRAWPANEL,&RInfo);
            break;
          }
        }
        break;
    }
  }
  return(INVALID_HANDLE_VALUE);
}
