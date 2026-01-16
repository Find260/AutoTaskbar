#include "SystemTray.h"
#include <tchar.h>

UINT SystemTray::s_wmTaskbarCreated = 0;

std::wstring SystemTray::GetConfigPath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring sPath = path;
    return sPath.substr(0, sPath.find_last_of(L"\\/")) + L"\\config.json";
}


// 保存配置
void SystemTray::SaveConfig() {
    std::wofstream file(GetConfigPath());
    if (file.is_open()) {
        file << L"{\n"
                << L"  \"desktopMode\": " << m_desktopMode << L",\n"
                << L"  \"maxMode\": " << m_maxMode << L",\n"
                << L"  \"touchMode\": " << m_touchMode << L",\n"
                << L"  \"callMode\": " << m_callMode << L",\n"
                << L"  \"hotkey\": \"" << m_hotkeyStr << L"\"\n"
                << L"}";
        file.close();
    }
}

void SystemTray::LoadConfig() {
    std::wifstream file(GetConfigPath());
    if (file.is_open()) {
        std::wstring line;
        while (std::getline(file, line)) {
            if (line.find(L"desktopMode") != std::wstring::npos)
                swscanf_s(line.c_str(), L"  \"desktopMode\": %d", &m_desktopMode);
            else if (line.find(L"maxMode") != std::wstring::npos)
                swscanf_s(line.c_str(), L"  \"maxMode\": %d", &m_maxMode);
            else if (line.find(L"touchMode") != std::wstring::npos)
                swscanf_s(line.c_str(), L"  \"touchMode\": %d", &m_touchMode);
            else if (line.find(L"callMode") != std::wstring::npos)
                swscanf_s(line.c_str(), L"  \"callMode\": %d", &m_callMode);
            else if (line.find(L"hotkey") != std::wstring::npos) {
                size_t start = line.find(L": \"") + 3;
                size_t end = line.find_last_of(L"\"");
                if (start != std::wstring::npos && end > start)
                    m_hotkeyStr = line.substr(start, end - start);
            }
        }
        file.close();
    }
}

// 快捷键对话框
/*
INT_PTR CALLBACK SystemTray::HotkeyDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static SystemTray *pThis = nullptr;
    static HWND hEdit = nullptr;
    switch (msg) {
        case WM_INITDIALOG:
            pThis = (SystemTray *) lParam;
            SetWindowTextW(hDlg, L"设置热键");
            CreateWindowExW(0, L"STATIC", L"请直接按下键盘组合键：", WS_CHILD | WS_VISIBLE, 20, 15, 150, 20, hDlg, nullptr,
                            nullptr, nullptr);
            hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pThis->m_hotkeyStr.c_str(),
                                    WS_CHILD | WS_VISIBLE | ES_CENTER | ES_READONLY, 20, 40, 160, 25, hDlg, nullptr,
                                    nullptr,
                                    nullptr);
            CreateWindowExW(0, L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE, 30, 80, 60, 25, hDlg, (HMENU) IDOK, nullptr,
                            nullptr);
            CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE, 110, 80, 60, 25, hDlg, (HMENU) IDCANCEL,
                            nullptr,
                            nullptr);
            return (INT_PTR) TRUE;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            std::wstring mods = L"";
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mods += L"Ctrl+";
            if (GetAsyncKeyState(VK_MENU) & 0x8000) mods += L"Alt+";
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) mods += L"Shift+";
            wchar_t name[64];
            UINT sc = (lParam >> 16) & 0xFF;
            GetKeyNameTextW(sc << 16, name, 64);
            if (wcscmp(name, L"Control") != 0 && wcscmp(name, L"Alt") != 0 && wcscmp(name, L"Shift") != 0)
                SetWindowTextW(hEdit, (mods + name).c_str());
            return (INT_PTR) TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                wchar_t buf[128];
                GetWindowTextW(hEdit, buf, 128);
                pThis->m_hotkeyStr = buf;
                EndDialog(hDlg, IDOK);
            }
            else if (LOWORD(wParam) == IDCANCEL) EndDialog(hDlg, IDCANCEL);
            break;
    }
    return (INT_PTR) FALSE;
}
*/

void SystemTray::ShowContextMenu(HWND hWnd) {
    HMENU hMenu = CreatePopupMenu();
    auto AddTaskbarMenu = [&](const wchar_t *title, int baseID, int currentMode) {
        HMENU hSub = CreatePopupMenu();
        AppendMenuW(hSub, (currentMode == 0) ? MF_CHECKED : MF_UNCHECKED, baseID + 0, L"显示");
        AppendMenuW(hSub, (currentMode == 1) ? MF_CHECKED : MF_UNCHECKED, baseID + 1, L"自动隐藏");
        AppendMenuW(hSub, (currentMode == 2) ? MF_CHECKED : MF_UNCHECKED, baseID + 2, L"完全隐藏");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR) hSub, title);
    };

    AddTaskbarMenu(L"在桌面时", IDM_SCENE_DESKTOP, m_desktopMode);
    AddTaskbarMenu(L"最大化/全屏时", IDM_SCENE_MAXIMIZED, m_maxMode);
    AddTaskbarMenu(L"窗口碰到任务栏时", IDM_SCENE_TOUCH, m_touchMode);

    HMENU hCall = CreatePopupMenu();
    AppendMenuW(hCall, (m_callMode == 0) ? MF_CHECKED : MF_UNCHECKED, IDM_SCENE_CALL + 0, L"永不呼出");
    AppendMenuW(hCall, (m_callMode == 1) ? MF_CHECKED : MF_UNCHECKED, IDM_SCENE_CALL + 1, L"底部左键");
    std::wstring hk = L"快捷键 (" + m_hotkeyStr + L")";
    AppendMenuW(hCall, (m_callMode == 2) ? MF_CHECKED : MF_UNCHECKED, IDM_SCENE_CALL + 2, hk.c_str());
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR) hCall, L"呼出任务栏");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED, IDM_AUTORUN, L"开机自启");
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"退出程序");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK SystemTray::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg != 0 && msg == s_wmTaskbarCreated) {
        getInstance().ReAddTray();
        TaskbarManager::getInstance().RefreshTaskbarInfo();
        TaskbarManager::getInstance().injector(true);
        return 0;
    }
    if (msg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) getInstance().ShowContextMenu(hWnd);
    }
    if (msg == WM_COMMAND) {
        int id = LOWORD(wParam);
        SystemTray &st = getInstance();
        if (id == IDM_EXIT) PostQuitMessage(0);
        else if (id == IDM_AUTORUN) st.SetAutoStart(!st.IsAutoStartEnabled());
        else if (id >= 1100 && id < 1500) {
            if (id < 1200) st.m_desktopMode = id - 1100;
            else if (id < 1300) st.m_maxMode = id - 1200;
            else if (id < 1400) st.m_touchMode = id - 1300;
            else {
                int mode = id - 1400;
                if (mode == 2) {
                    /*#pragma pack(push, 2)
                                        struct {
                                            WORD v;
                                            WORD s;
                                            DWORD h;
                                            DWORD e;
                                            DWORD st;
                                            WORD c;
                                            short x;
                                            short y;
                                            short cx;
                                            short cy;
                                        }
                                                t = {1, 0xFFFF, 0, 0, WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER, 0, 0, 0, 180,
                                                     100};
                    #pragma pack(pop)
                                        if (DialogBoxIndirectParamW(st.m_hInstance, (LPDLGTEMPLATE) &t, hWnd, HotkeyDlgProc,
                                                                    (LPARAM) &st) == IDOK)
                                            st.m_callMode = 2;*/
                }
                else st.m_callMode = mode;
            }
            st.SaveConfig();
            // 再次初始化任务栏
            TaskbarManager::getInstance().Init();
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void SystemTray::CreateTray(HINSTANCE h) {
    m_hInstance = h;
    LoadConfig();
    s_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = h;
    wc.lpszClassName = L"TrayClass";
    RegisterClassW(&wc);
    m_hwnd = CreateWindowExW(0, L"TrayClass", L"Tray", 0, 0, 0, 0, 0, nullptr, nullptr, h, nullptr);
    memset(&m_nid, 0, sizeof(m_nid));
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadIconW(h, MAKEINTRESOURCEW(IDI_ICON1));
    if (!m_nid.hIcon) m_nid.hIcon = LoadIconW(nullptr, (LPCWSTR) IDI_APPLICATION);
    Shell_NotifyIconW(NIM_ADD, &m_nid);
}

void SystemTray::ReAddTray() { Shell_NotifyIconW(NIM_ADD, &m_nid); }

void SystemTray::RemoveTray() { Shell_NotifyIconW(NIM_DELETE, &m_nid); }

bool SystemTray::IsAutoStartEnabled() {
    HKEY k;
    bool e = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &k) ==
        ERROR_SUCCESS) {
        e = (RegQueryValueExW(k, L"TaskbarController", nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
        RegCloseKey(k);
    }
    return e;
}

void SystemTray::SetAutoStart(bool b) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &k) ==
        ERROR_SUCCESS) {
        if (b) {
            wchar_t p[MAX_PATH];
            GetModuleFileNameW(nullptr, p, MAX_PATH);
            RegSetValueExW(k, L"TaskbarController", 0, REG_SZ, (BYTE *) p, (lstrlenW(p) + 1) * sizeof(wchar_t));
        }
        else RegDeleteValueW(k, L"TaskbarController");
        RegCloseKey(k);
    }
}
