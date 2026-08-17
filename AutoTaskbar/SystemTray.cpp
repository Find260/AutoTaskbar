#include "SystemTray.h"
#include <tchar.h>
#include <algorithm>
#include <limits>

UINT SystemTray::s_wmTaskbarCreated = 0;

namespace {
struct HotkeyCaptureState {
    std::array<DWORD, 3> keys{};
    std::array<bool, 256> down{};
    HWND display = nullptr;
    bool saved = false;
};

struct InjectionDelayState {
    int seconds = 60;
    HWND edit = nullptr;
    bool saved = false;
};
}

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
                << L"  \"injectionDelaySeconds\": " << m_injectionDelaySeconds << L",\n"
                << L"  \"hotkey\": \"" << m_hotkeyStr << L"\",\n"
                << L"  \"hotkeyKey1\": " << m_hotkeyKeys[0] << L",\n"
                << L"  \"hotkeyKey2\": " << m_hotkeyKeys[1] << L",\n"
                << L"  \"hotkeyKey3\": " << m_hotkeyKeys[2] << L"\n"
                << L"}";
        file.close();
    }
}

void SystemTray::LoadConfig() {
    std::wifstream file(GetConfigPath());
    bool hasNumericHotkey = false;
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
            else if (line.find(L"injectionDelaySeconds") != std::wstring::npos) {
                swscanf_s(line.c_str(), L"  \"injectionDelaySeconds\": %d", &m_injectionDelaySeconds);
            }
            else if (line.find(L"\"hotkey\"") != std::wstring::npos) {
                size_t start = line.find(L": \"") + 3;
                size_t end = line.find_last_of(L"\"");
                if (start != std::wstring::npos && end > start)
                    m_hotkeyStr = line.substr(start, end - start);
            }
            if (line.find(L"hotkeyKey1") != std::wstring::npos) {
                swscanf_s(line.c_str(), L"  \"hotkeyKey1\": %lu", &m_hotkeyKeys[0]);
                hasNumericHotkey = true;
            }
            else if (line.find(L"hotkeyKey2") != std::wstring::npos) {
                swscanf_s(line.c_str(), L"  \"hotkeyKey2\": %lu", &m_hotkeyKeys[1]);
                hasNumericHotkey = true;
            }
            else if (line.find(L"hotkeyKey3") != std::wstring::npos) {
                swscanf_s(line.c_str(), L"  \"hotkeyKey3\": %lu", &m_hotkeyKeys[2]);
                hasNumericHotkey = true;
            }
        }
        file.close();
    }
    if (m_injectionDelaySeconds < 0) {
        m_injectionDelaySeconds = 60;
    }
    if (!hasNumericHotkey ||
        (m_hotkeyKeys[0] == 0 && m_hotkeyKeys[1] == 0 && m_hotkeyKeys[2] == 0)) {
        m_hotkeyKeys = {VK_LWIN, 0, 0};
    }
    for (auto& key : m_hotkeyKeys) {
        key = NormalizeHotkey(key);
    }
    m_hotkeyStr = FormatHotkey(m_hotkeyKeys);
}

DWORD SystemTray::NormalizeHotkey(DWORD vkCode) {
    switch (vkCode) {
        case VK_LCONTROL:
        case VK_RCONTROL:
            return VK_CONTROL;
        case VK_LSHIFT:
        case VK_RSHIFT:
            return VK_SHIFT;
        case VK_LMENU:
        case VK_RMENU:
            return VK_MENU;
        case VK_RWIN:
            return VK_LWIN;
        default:
            return vkCode;
    }
}

std::wstring SystemTray::GetKeyName(DWORD vkCode) {
    switch (vkCode) {
        case VK_CONTROL: return L"Ctrl";
        case VK_SHIFT: return L"Shift";
        case VK_MENU: return L"Alt";
        case VK_LWIN: return L"Win";
        case VK_SPACE: return L"Space";
        case VK_RETURN: return L"Enter";
        case VK_ESCAPE: return L"Esc";
        case VK_TAB: return L"Tab";
        case VK_BACK: return L"Backspace";
        case VK_DELETE: return L"Delete";
    }
    if ((vkCode >= L'0' && vkCode <= L'9') ||
        (vkCode >= L'A' && vkCode <= L'Z')) {
        return std::wstring(1, static_cast<wchar_t>(vkCode));
    }
    if (vkCode >= VK_F1 && vkCode <= VK_F24) {
        return L"F" + std::to_wstring(vkCode - VK_F1 + 1);
    }

    UINT scanCode = MapVirtualKeyW(vkCode, MAPVK_VK_TO_VSC);
    LONG keyData = static_cast<LONG>(scanCode << 16);
    switch (vkCode) {
        case VK_INSERT:
        case VK_HOME:
        case VK_PRIOR:
        case VK_DELETE:
        case VK_END:
        case VK_NEXT:
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
            keyData |= 1 << 24;
    }
    wchar_t name[64] = {};
    if (GetKeyNameTextW(keyData, name, 64) > 0) {
        return name;
    }
    return L"VK_" + std::to_wstring(vkCode);
}

std::wstring SystemTray::FormatHotkey(const std::array<DWORD, 3>& keys) {
    std::wstring result;
    for (DWORD key : keys) {
        if (key == 0) {
            continue;
        }
        if (!result.empty()) {
            result += L"+";
        }
        result += GetKeyName(key);
    }
    return result.empty() ? L"未设置" : result;
}

LRESULT CALLBACK SystemTray::HotkeyWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<HotkeyCaptureState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = static_cast<HotkeyCaptureState*>(create->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }
    if (msg == WM_CREATE) {
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HWND instruction = CreateWindowExW(0, L"STATIC",
                L"请直接按下 1 至 3 个键组成的快捷键：",
                WS_CHILD | WS_VISIBLE, 20, 18, 360, 22,
                hWnd, nullptr, nullptr, nullptr);
        state->display = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"未设置",
                WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
                20, 48, 360, 34, hWnd,
                reinterpret_cast<HMENU>(IDC_HOTKEY_DISPLAY), nullptr, nullptr);
        HWND save = CreateWindowExW(0, L"BUTTON", L"保存",
                WS_CHILD | WS_VISIBLE, 65, 100, 80, 28, hWnd,
                reinterpret_cast<HMENU>(IDC_HOTKEY_SAVE), nullptr, nullptr);
        HWND clear = CreateWindowExW(0, L"BUTTON", L"清除",
                WS_CHILD | WS_VISIBLE, 160, 100, 80, 28, hWnd,
                reinterpret_cast<HMENU>(IDC_HOTKEY_CLEAR), nullptr, nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消",
                WS_CHILD | WS_VISIBLE, 255, 100, 80, 28, hWnd,
                reinterpret_cast<HMENU>(IDC_HOTKEY_CANCEL), nullptr, nullptr);
        for (HWND control : {instruction, state->display, save, clear, cancel}) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        return 0;
    }
    if (msg == WM_COMMAND && state) {
        switch (LOWORD(wParam)) {
            case IDC_HOTKEY_SAVE:
                if (state->keys[0] == 0) {
                    MessageBoxW(hWnd, L"请先按下快捷键。", L"快捷键", MB_OK | MB_ICONINFORMATION);
                    SetFocus(hWnd);
                    return 0;
                }
                state->saved = true;
                DestroyWindow(hWnd);
                return 0;
            case IDC_HOTKEY_CLEAR:
                state->keys = {};
                state->down = {};
                SetWindowTextW(state->display, L"未设置");
                SetFocus(hWnd);
                return 0;
            case IDC_HOTKEY_CANCEL:
                DestroyWindow(hWnd);
                return 0;
        }
    }
    if (msg == WM_CLOSE) {
        DestroyWindow(hWnd);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool SystemTray::ShowHotkeyDialog(HWND owner) {
    const wchar_t* className = L"AutoTaskbarHotkeyCapture";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = HotkeyWndProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = className;
    RegisterClassW(&wc);

    HotkeyCaptureState state;
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, className, L"设置呼出快捷键",
            WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 420, 180,
            owner, nullptr, m_hInstance, &state);
    if (!window) {
        return false;
    }

    int previousCallMode = TaskbarManager::callSetting.load();
    TaskbarManager::callSetting = 0;
    EnableWindow(owner, FALSE);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    SetFocus(window);

    MSG msg;
    while (IsWindow(window) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN ||
             msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP) &&
            (msg.hwnd == window || IsChild(window, msg.hwnd))) {
            DWORD key = NormalizeHotkey(static_cast<DWORD>(msg.wParam));
            bool keyDown = msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN;
            if (key < state.down.size()) {
                if (keyDown && !state.down[key]) {
                    bool alreadyAdded = false;
                    for (DWORD savedKey : state.keys) {
                        alreadyAdded = alreadyAdded || savedKey == key;
                    }
                    if (!alreadyAdded) {
                        auto empty = std::find(state.keys.begin(), state.keys.end(), DWORD{0});
                        if (empty != state.keys.end()) {
                            *empty = key;
                            SetWindowTextW(state.display, FormatHotkey(state.keys).c_str());
                        }
                        else {
                            MessageBeep(MB_ICONWARNING);
                        }
                    }
                }
                state.down[key] = keyDown;
            }
            SetFocus(window);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    TaskbarManager::callSetting = previousCallMode;
    if (state.saved) {
        m_hotkeyKeys = state.keys;
        m_hotkeyStr = FormatHotkey(m_hotkeyKeys);
    }
    return state.saved;
}

LRESULT CALLBACK SystemTray::InjectionDelayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<InjectionDelayState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = static_cast<InjectionDelayState*>(create->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }
    if (msg == WM_CREATE) {
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HWND instruction = CreateWindowExW(0, L"STATIC",
                L"启动后延迟注入和操作任务栏的时间（秒）：",
                WS_CHILD | WS_VISIBLE, 20, 20, 360, 22,
                hWnd, nullptr, nullptr, nullptr);
        state->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                std::to_wstring(state->seconds).c_str(),
                WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER | ES_AUTOHSCROLL,
                120, 52, 160, 28, hWnd,
                reinterpret_cast<HMENU>(IDC_DELAY_EDIT), nullptr, nullptr);
        HWND hint = CreateWindowExW(0, L"STATIC", L"请输入大于等于 0 的整数；0 表示不延迟。",
                WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 88, 360, 22,
                hWnd, nullptr, nullptr, nullptr);
        HWND save = CreateWindowExW(0, L"BUTTON", L"保存",
                WS_CHILD | WS_VISIBLE, 110, 120, 80, 28, hWnd,
                reinterpret_cast<HMENU>(IDC_DELAY_SAVE), nullptr, nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消",
                WS_CHILD | WS_VISIBLE, 210, 120, 80, 28, hWnd,
                reinterpret_cast<HMENU>(IDC_DELAY_CANCEL), nullptr, nullptr);
        for (HWND control : {instruction, state->edit, hint, save, cancel}) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        SendMessageW(state->edit, EM_SETLIMITTEXT, 10, 0);
        SetFocus(state->edit);
        SendMessageW(state->edit, EM_SETSEL, 0, -1);
        return 0;
    }
    if (msg == WM_COMMAND && state) {
        if (LOWORD(wParam) == IDC_DELAY_SAVE) {
            wchar_t value[16] = {};
            GetWindowTextW(state->edit, value, 16);
            wchar_t* end = nullptr;
            long long seconds = wcstoll(value, &end, 10);
            if (value[0] == L'\0' || !end || *end != L'\0' || seconds < 0 ||
                seconds > (std::numeric_limits<int>::max)()) {
                MessageBoxW(hWnd, L"请输入大于等于 0 的有效整数。", L"延迟注入等待",
                        MB_OK | MB_ICONWARNING);
                SetFocus(state->edit);
                SendMessageW(state->edit, EM_SETSEL, 0, -1);
                return 0;
            }
            state->seconds = static_cast<int>(seconds);
            state->saved = true;
            DestroyWindow(hWnd);
            return 0;
        }
        if (LOWORD(wParam) == IDC_DELAY_CANCEL) {
            DestroyWindow(hWnd);
            return 0;
        }
    }
    if (msg == WM_CLOSE) {
        DestroyWindow(hWnd);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool SystemTray::ShowInjectionDelayDialog(HWND owner) {
    const wchar_t* className = L"AutoTaskbarInjectionDelay";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = InjectionDelayWndProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = className;
    RegisterClassW(&wc);

    InjectionDelayState state;
    state.seconds = m_injectionDelaySeconds;
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, className, L"设置延迟注入等待",
            WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 420, 205,
            owner, nullptr, m_hInstance, &state);
    if (!window) {
        return false;
    }

    EnableWindow(owner, FALSE);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG msg;
    while (IsWindow(window) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (state.saved) {
        m_injectionDelaySeconds = state.seconds;
    }
    return state.saved;
}

void SystemTray::SetInjectionDelayStatus(int remainingSeconds) {
    m_injectionRemainingSeconds = remainingSeconds;
    m_injectionStatusUpdatedAt = GetTickCount64();
    std::wstring tip;
    if (remainingSeconds > 0) {
        tip = L"AutoTaskbar - 延迟注入，剩余 " + std::to_wstring(remainingSeconds) + L" 秒";
    }
    else {
        tip = L"AutoTaskbar";
    }
    wcsncpy_s(m_nid.szTip, tip.c_str(), _TRUNCATE);
    m_nid.uFlags |= NIF_TIP;
    if (m_hwnd) {
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }
    UpdateInjectionStatusMenu();
}

void SystemTray::InitializeTaskbarManager() {
    if (m_taskbarManagerReady) {
        return;
    }
    TaskbarManager& mgr = TaskbarManager::getInstance();
    mgr.RefreshTaskbarInfo();
    if (mgr.CanAdjustTaskbar()) {
        TaskbarMode initialMode = mgr.ShouldTaskbarHide();
        mgr.ControlTaskbarLock(initialMode);
    }
    m_taskbarManagerReady = true;
    SetInjectionDelayStatus(0);
}

int SystemTray::GetCurrentInjectionRemainingSeconds() const {
    if (m_injectionRemainingSeconds <= 0) {
        return 0;
    }
    ULONGLONG elapsedSeconds = (GetTickCount64() - m_injectionStatusUpdatedAt) / 1000;
    if (elapsedSeconds >= static_cast<ULONGLONG>(m_injectionRemainingSeconds)) {
        return 0;
    }
    return m_injectionRemainingSeconds - static_cast<int>(elapsedSeconds);
}

std::wstring SystemTray::GetInjectionStatusText() const {
    if (m_taskbarManagerReady) {
        return TaskbarManager::getInstance().isInjected().load()
                ? L"注入状态：已注入"
                : L"注入状态：未注入";
    }
    return L"注入状态：倒计时 " +
            std::to_wstring(GetCurrentInjectionRemainingSeconds()) + L" 秒";
}

void SystemTray::UpdateInjectionStatusMenu() {
    if (!m_activeMenu) {
        return;
    }
    std::wstring status = GetInjectionStatusText();
    ModifyMenuW(m_activeMenu, IDM_INJECTION_STATUS,
            MF_BYCOMMAND | MF_STRING | MF_DISABLED,
            IDM_INJECTION_STATUS, status.c_str());
    if (m_hwnd) {
        DrawMenuBar(m_hwnd);
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
    std::wstring injectionStatus = GetInjectionStatusText();
    AppendMenuW(hMenu, MF_STRING | MF_DISABLED, IDM_INJECTION_STATUS,
            injectionStatus.c_str());
    std::wstring waitTitle = L"延迟注入等待 (" +
            std::to_wstring(m_injectionDelaySeconds) + L" 秒)...";
    AppendMenuW(hMenu, MF_STRING, IDM_INJECTION_DELAY, waitTitle.c_str());
    AppendMenuW(hMenu, IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED, IDM_AUTORUN, L"开机自启");
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"退出程序");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hWnd);
    m_activeMenu = hMenu;
    SetTimer(hWnd, IDT_INJECTION_STATUS, 1000, nullptr);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
    KillTimer(hWnd, IDT_INJECTION_STATUS);
    m_activeMenu = nullptr;
    DestroyMenu(hMenu);
}

LRESULT CALLBACK SystemTray::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg != 0 && msg == s_wmTaskbarCreated) {
        SystemTray& tray = getInstance();
        tray.ReAddTray();
        if (tray.m_taskbarManagerReady) {
            TaskbarManager::getInstance().RefreshTaskbarInfo();
            TaskbarManager::getInstance().injector(true);
        }
        return 0;
    }
    if (msg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) getInstance().ShowContextMenu(hWnd);
    }
    if (msg == WM_TIMER && wParam == IDT_INJECTION_STATUS) {
        SystemTray& tray = getInstance();
        if (!tray.m_taskbarManagerReady &&
            tray.GetCurrentInjectionRemainingSeconds() == 0) {
            tray.InitializeTaskbarManager();
        }
        tray.UpdateInjectionStatusMenu();
        return 0;
    }
    if (msg == WM_COMMAND) {
        int id = LOWORD(wParam);
        SystemTray &st = getInstance();
        if (id == IDM_EXIT) PostQuitMessage(0);
        else if (id == IDM_AUTORUN) st.SetAutoStart(!st.IsAutoStartEnabled());
        else if (id == IDM_INJECTION_DELAY) {
            if (st.ShowInjectionDelayDialog(hWnd)) {
                st.SaveConfig();
            }
        }
        else if (id >= 1100 && id < 1500) {
            if (id < 1200) st.m_desktopMode = id - 1100;
            else if (id < 1300) st.m_maxMode = id - 1200;
            else if (id < 1400) st.m_touchMode = id - 1300;
            else {
                int mode = id - 1400;
                if (mode == 2) {
                    if (!st.ShowHotkeyDialog(hWnd)) {
                        return 0;
                    }
                    st.m_callMode = 2;
                }
                else st.m_callMode = mode;
            }
            st.SaveConfig();
            // 再次初始化任务栏
            if (st.m_taskbarManagerReady) {
                TaskbarManager::getInstance().Init();
            }
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void SystemTray::CreateTray(HINSTANCE h) {
    m_hInstance = h;
    LoadConfig();
    m_injectionRemainingSeconds = m_injectionDelaySeconds;
    m_injectionStatusUpdatedAt = GetTickCount64();
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
    wcsncpy_s(m_nid.szTip, L"AutoTaskbar", _TRUNCATE);
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
            std::wstring command = L"\"" + std::wstring(p) + L"\"";
            RegSetValueExW(
                k,
                L"TaskbarController",
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(command.c_str()),
                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))
            );
        }
        else RegDeleteValueW(k, L"TaskbarController");
        RegCloseKey(k);
    }
}

//atb-tmp