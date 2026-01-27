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
                    st.m_callMode = 2;
                    /*
                     * 快捷键修改逻辑
                     */
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
