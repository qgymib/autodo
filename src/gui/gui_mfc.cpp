#include <uv.h>
#include "gui.h"
#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#include <resource.h>
#include <strsafe.h>

static TCHAR s_window_class[] = _T("AutoDo");
static TCHAR s_window_title[] = _T("Automation Tool");

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

int auto_gui(auto_gui_startup_info_t* info)
{
    WNDCLASSEX wcex;
    ZeroMemory(&wcex, sizeof(wcex));

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = GetModuleHandle(NULL);
    wcex.hIcon = LoadIcon(wcex.hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = s_window_class;
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL,
            _T("Call to RegisterClassEx failed!"),
            _T("Windows Desktop Guided Tour"),
            NULL);

        return EXIT_FAILURE;
    }

    HWND hWnd = CreateWindowEx(
        WS_EX_OVERLAPPEDWINDOW,
        s_window_class,
        s_window_title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 100,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (!hWnd)
    {
        MessageBox(NULL,
            _T("Call to CreateWindow failed!"),
            _T("Windows Desktop Guided Tour"),
            NULL);

        return EXIT_FAILURE;
    }

    ShowWindow(hWnd, info->nShowCmd);
    UpdateWindow(hWnd);

    {
        NOTIFYICONDATA nid;
        ZeroMemory(&nid, sizeof(nid));
        nid.cbSize = sizeof(nid);
        nid.uFlags = NIF_ICON | NIF_GUID | NIF_TIP;
        static const GUID myGUID =
        { 0x23977b55, 0x10e0, 0x4041, {0xb8, 0x62, 0xb1, 0x95, 0x41, 0x96, 0x36, 0x69} };
        nid.guidItem = myGUID;
        StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), "Test application");
#if 0
        LoadIconMetric(wcex.hInstance, MAKEINTRESOURCEW(IDI_ICON1), LIM_SMALL, &nid.hIcon);
#else
        if ((nid.hIcon = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON1))) == NULL)
        {
            abort();
        }
#endif
        Shell_NotifyIcon(NIM_ADD, &nid);

        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIcon(NIM_SETVERSION, &nid);
    }

#if 0
    {
        auto_gui_msg_t msg;
        msg.type = AUTO_GUI_READY;
        info->on_event(&msg, info->udata);
    }
#endif

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return EXIT_SUCCESS;
}

void auto_gui_exit(void)
{
    PostQuitMessage(0);
}
