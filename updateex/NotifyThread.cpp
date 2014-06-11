#define WM_TRAY_TRAYMSG WM_APP + 0x00001000
#define NOTIFY_DURATION 10000

LRESULT CALLBACK tray_wnd_proc(HWND wnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
	if (msg == WM_TRAY_TRAYMSG && l_param == WM_LBUTTONDBLCLK)
		PostQuitMessage(0);
	return DefWindowProc(wnd, msg, w_param, l_param);
}

DWORD WINAPI NotifyProc(LPVOID)
{
	HICON tray_icon=nullptr;
	HWND tray_wnd=nullptr;
	WNDCLASSEX tray_wc;
	NOTIFYICONDATA tray_icondata;

	ZeroMemory(&tray_wc, sizeof(tray_wc));
	ZeroMemory(&tray_icondata, sizeof(tray_icondata));

	tray_icon=ExtractIcon(GetModuleHandle(nullptr), Info.ModuleName, 0);

	tray_wc.cbSize=sizeof(WNDCLASSEX);
	tray_wc.style=CS_HREDRAW|CS_VREDRAW;
	tray_wc.lpfnWndProc=&tray_wnd_proc;
	tray_wc.cbClsExtra=0;
	tray_wc.cbWndExtra=0;
	tray_wc.hInstance=GetModuleHandle(nullptr);
	tray_wc.hIcon=tray_icon;
	tray_wc.hCursor=LoadCursor(nullptr, IDC_ARROW);
	tray_wc.hbrBackground=(HBRUSH)(COLOR_WINDOW + 1);
	tray_wc.lpszMenuName=nullptr;
	tray_wc.lpszClassName=L"UpdateNotifyClass";
	tray_wc.hIconSm=tray_icon;

	if (RegisterClassEx(&tray_wc))
	{
		tray_wnd=CreateWindow(tray_wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
		if (tray_wnd)
		{
			tray_icondata.cbSize=NOTIFYICONDATA_V3_SIZE;//sizeof(NOTIFYICONDATA);
			tray_icondata.uID=1;
			tray_icondata.hWnd=tray_wnd;
			tray_icondata.uFlags=NIF_ICON|NIF_MESSAGE|NIF_INFO|NIF_TIP;
			tray_icondata.hIcon=tray_icon;
			tray_icondata.uTimeout=NOTIFY_DURATION;
			tray_icondata.dwInfoFlags=NIIF_INFO|NIIF_LARGE_ICON;
			FSF.sprintf(tray_icondata.szInfo,MSG(MTrayNotify),CountDownload,CountUpdate);
			lstrcpy(tray_icondata.szInfoTitle, MSG(MName));
			lstrcpy(tray_icondata.szTip,tray_icondata.szInfo);
			tray_icondata.uCallbackMessage=WM_TRAY_TRAYMSG;

			if (Shell_NotifyIcon(NIM_ADD, &tray_icondata))
			{
				size_t n=CountDownload;
				for (;;)
				{
					if (n!=CountDownload)
					{
						n++;
						FSF.sprintf(tray_icondata.szInfo,MSG(MTrayNotify),CountDownload,CountUpdate);
						lstrcpy(tray_icondata.szInfoTitle, MSG(MName));
						lstrcpy(tray_icondata.szTip,tray_icondata.szInfo);
						Shell_NotifyIcon(NIM_MODIFY, &tray_icondata);
					}
					MSG msg;
					if (PeekMessage(&msg,nullptr,0,0,PM_NOREMOVE))
					{
						GetMessage(&msg,nullptr,0,0);
						if (msg.message == WM_CLOSE)
							break;
					}
					if (GetStatus()!=S_DOWNLOAD)
					{
						PostQuitMessage(0);
						break;
					}
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			Shell_NotifyIcon(NIM_DELETE, &tray_icondata);
			DestroyWindow(tray_wnd);
			tray_wnd=nullptr;
		}
	}
	if (tray_icon) DestroyIcon(tray_icon);
	UnregisterClass(tray_wc.lpszClassName, GetModuleHandle(nullptr));
	CloseHandle(hNotifyThread);
	hNotifyThread=nullptr;
	return 0;
}
