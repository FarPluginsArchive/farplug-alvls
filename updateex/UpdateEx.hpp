#pragma once

#define ControlKeyAllMask (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED|RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED|SHIFT_PRESSED)
#define ControlKeyAltMask (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED)
#define ControlKeyNonAltMask (RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED|SHIFT_PRESSED)
#define ControlKeyCtrlMask (RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED)
#define ControlKeyNonCtrlMask (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED|SHIFT_PRESSED)
#define IsShift(rec) (((rec)->Event.KeyEvent.dwControlKeyState&ControlKeyAllMask)==SHIFT_PRESSED)
#define IsAlt(rec) (((rec)->Event.KeyEvent.dwControlKeyState&ControlKeyAltMask)&&!((rec)->Event.KeyEvent.dwControlKeyState&ControlKeyNonAltMask))
#define IsCtrl(rec) (((rec)->Event.KeyEvent.dwControlKeyState&ControlKeyCtrlMask)&&!((rec)->Event.KeyEvent.dwControlKeyState&ControlKeyNonCtrlMask))
#define IsNone(rec) (((rec)->Event.KeyEvent.dwControlKeyState&ControlKeyAllMask)==0)

#define MSG(ID) Info.GetMsg(&MainGuid, ID)

#define DN_UPDDLG DM_USER+1

#define MIN_FAR_MAJOR_VER 3
#define MIN_FAR_MINOR_VER 0
#define MIN_FAR_BUILD     2927

#define MAJOR_VER 3
#define MINOR_VER 1
#define BUILD 34

#define _W(arg) L##arg
#define _STR(arg) _W(#arg)
#define STR(arg) _STR(arg)

#define FARVER L"3.0"

#ifdef _WIN64
#define PLATFORM L" x64"
#else
#define PLATFORM L" x86"
#endif

#define ALLSTR STR(MAJOR_VER) L"." STR(MINOR_VER) L" build " STR(BUILD) PLATFORM

#define PRODUCTNAME L"UpdateEx"

