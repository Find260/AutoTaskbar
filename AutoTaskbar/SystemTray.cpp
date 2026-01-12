#include "SystemTray.h"
#include <tchar.h>
#include <cstdio>

UINT SystemTray::s_wmTaskbarCreated = 0;

void SystemTray::CreateTray(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    s_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TaskbarTrayClass";
    RegisterClassW(&wc);

    // 使用 HWND_MESSAGE 创建纯消息窗口，彻底杜绝开机时的窗口闪烁
    m_hwnd = CreateWindowExW(0, L"TaskbarTrayClass", L"TaskbarController", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);

    memset(&m_nid, 0, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;

    // 尝试加载自定义图标，失败则使用系统默认图标
    m_nid.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));
    if (!m_nid.hIcon) {
        m_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }

    lstrcpyW(m_nid.szTip, L"AutoTaskbar");

    Shell_NotifyIconW(NIM_ADD, &m_nid);
}

void SystemTray::ReAddTray() {
    if (m_hwnd) {
        Shell_NotifyIconW(NIM_ADD, &m_nid);
    }
}

void SystemTray::RemoveTray() {
    Shell_NotifyIconW(NIM_DELETE, &m_nid);
}

bool SystemTray::IsAutoStartEnabled() {
    HKEY hKey;
    bool enabled = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        // 检查值是否存在即可
        enabled = (RegQueryValueExW(hKey, L"TaskbarController", NULL, NULL, NULL, NULL) == ERROR_SUCCESS);
        RegCloseKey(hKey);
    }
    return enabled;
}

void SystemTray::SetAutoStart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);

            wchar_t command[MAX_PATH + 12];
            swprintf_s(command, L"\"%s\" -delay", path);

            RegSetValueExW(hKey, L"TaskbarController", 0, REG_SZ, (BYTE*)command, (lstrlenW(command) + 1) * sizeof(wchar_t));
        } else {
            RegDeleteValueW(hKey, L"TaskbarController");
        }
        RegCloseKey(hKey);
    }
}

LRESULT CALLBACK SystemTray::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg != 0 && msg == s_wmTaskbarCreated) {
        getInstance().ReAddTray();
        return 0;
    }

    if (msg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            bool isAuto = getInstance().IsAutoStartEnabled();

            AppendMenuW(hMenu, isAuto ? MF_CHECKED : MF_UNCHECKED, IDM_AUTORUN, L"开机自启");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"退出程序");

            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
        }
    }

    if (msg == WM_COMMAND) {
        if (LOWORD(wParam) == IDM_EXIT) PostQuitMessage(0);
        if (LOWORD(wParam) == IDM_AUTORUN) getInstance().SetAutoStart(!getInstance().IsAutoStartEnabled());
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}