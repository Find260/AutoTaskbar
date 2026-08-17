#include "TaskbarManager.h"

HMODULE GetRemoteModuleHandle(DWORD pid, const wchar_t *moduleName) {
    HANDLE hSnap =
            CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnap == INVALID_HANDLE_VALUE)
        return nullptr;

    MODULEENTRY32W me = {sizeof(me)};
    if (Module32FirstW(hSnap, &me)) {
        do {
            if (_wcsicmp(me.szModule, moduleName) == 0) {
                CloseHandle(hSnap);
                return (HMODULE) me.modBaseAddr;
            }
        }
        while (Module32NextW(hSnap, &me));
    }
    CloseHandle(hSnap);
    return nullptr;
}

int TaskbarManager::ModeSetting = 10; // 默认
std::atomic<int> TaskbarManager::callSetting{0};
TaskbarMode TaskbarManager::lastMode;
std::array<std::atomic<DWORD>, 3> TaskbarManager::HotkeyKeys{
        VK_LWIN, 0, 0
};

std::wstring TaskbarManager::GetConfigPath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring sPath = path;
    return sPath.substr(0, sPath.find_last_of(L"\\/")) + L"\\config.json";
}

int TaskbarManager::ParseJsonValue(const std::wstring &content,
                                   const std::wstring &key) {
    size_t pos = content.find(L"\"" + key + L"\"");
    if (pos == std::wstring::npos)
        return 0;
    size_t colon = content.find(L":", pos);
    if (colon == std::wstring::npos)
        return 0;
    return _wtoi(content.substr(colon + 1).c_str());
}

void TaskbarManager::LoadConfig() {
    std::wifstream file(GetConfigPath());
    if (file.is_open()) {
        std::wstring content((std::istreambuf_iterator<wchar_t>(file)),
                             std::istreambuf_iterator<wchar_t>());
        file.close();

        int desktop = ParseJsonValue(content, L"desktopMode");
        int maximized = ParseJsonValue(content, L"maxMode");
        int touch = ParseJsonValue(content, L"touchMode");
        int call = ParseJsonValue(content, L"callMode");
        DWORD hotkey1 = ParseJsonValue(content, L"hotkeyKey1");
        DWORD hotkey2 = ParseJsonValue(content, L"hotkeyKey2");
        DWORD hotkey3 = ParseJsonValue(content, L"hotkeyKey3");
        int mode = 0;
        mode |= ((desktop & 0x03) << 4);
        mode |= ((maximized & 0x03) << 2);
        mode |= ((touch & 0x03));
        ModeSetting = mode;
        callSetting = (call & 0x03);
        // 兼容旧配置：没有数值键码时继续使用原来的 Win 键。
        if (hotkey1 == 0 && hotkey2 == 0 && hotkey3 == 0) {
            hotkey1 = VK_LWIN;
        }
        HotkeyKeys[0] = NormalizeHotkey(hotkey1);
        HotkeyKeys[1] = NormalizeHotkey(hotkey2);
        HotkeyKeys[2] = NormalizeHotkey(hotkey3);
    }
}

void TaskbarManager::SaveConfig() {
    std::wofstream file(GetConfigPath());
    if (file.is_open()) {
        file << L"{\n"
                << L"  \"desktopMode\": " << 0 << L",\n"
                << L"  \"maxMode\": " << 2 << L",\n"
                << L"  \"touchMode\": " << 2 << L",\n"
                << L"  \"callMode\": " << 1 << L",\n"
                << L"  \"injectionDelaySeconds\": 60,\n"
                << L"  \"hotkey\": \"" << "Win" << L"\",\n"
                << L"  \"hotkeyKey1\": " << VK_LWIN << L",\n"
                << L"  \"hotkeyKey2\": 0,\n"
                << L"  \"hotkeyKey3\": 0\n"
                << L"}\n";
        file.close();
    }
}

// 鼠标钩子
LRESULT CALLBACK TaskbarManager::MouseProc(int nCode, WPARAM wParam,
                                           LPARAM lParam) {
    if (nCode >= 0) {
        auto &mgr = TaskbarManager::getInstance();
        if (wParam == WM_LBUTTONDOWN) {
            //std::cout << "点击事件" << std::endl;112@
            MSLLHOOKSTRUCT *pMouseStruct = (MSLLHOOKSTRUCT *) lParam;
            POINT pt = pMouseStruct->pt;
            //std::cout << "点击位置: " << pt.x << ", " << pt.y << " " <<
            // mgr.g_taskbarRect.top << std::endl;
            if (pt.y >= mgr.g_taskbarRect.bottom - 2 && !mgr.isPaused() &&
                mgr.callSetting == 1) {
                lastMode = mgr.currentMode().load();
                // 暂停任务栏的调整
                mgr.isPaused() = true;
                mgr.ControlTaskbarLock(MODE_ALWAYS_SHOW);
                std::thread([&mgr]() {
                    while (mgr.isPaused()) {
                        Sleep(50);
                    }
                }).detach();
            }
            // 点击任务栏外, 且是暂停状态
            else if (!PtInRect(&mgr.g_taskbarRect, pt) && mgr.isPaused()) {
                mgr.isPaused() = false;
                mgr.ControlTaskbarLock(lastMode);
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

DWORD TaskbarManager::NormalizeHotkey(DWORD vkCode) {
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

bool TaskbarManager::IsHotkeyPressed(const std::array<bool, 256>& pressedKeys) {
    bool hasKey = false;
    for (const auto& configuredKey : HotkeyKeys) {
        DWORD key = configuredKey.load();
        if (key == 0) {
            continue;
        }
        hasKey = true;
        if (key >= pressedKeys.size() || !pressedKeys[key]) {
            return false;
        }
    }
    return hasKey;
}

// 键盘钩子
LRESULT CALLBACK TaskbarManager::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    static std::array<bool, 256> pressedKeys{};
    static bool hotkeyLatched = false;

    if (nCode == HC_ACTION) {
        auto* keyInfo = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        DWORD key = NormalizeHotkey(keyInfo->vkCode);
        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        if (key < pressedKeys.size() && (isKeyDown || isKeyUp)) {
            pressedKeys[key] = isKeyDown;
            bool hotkeyPressed = IsHotkeyPressed(pressedKeys);

            if (hotkeyPressed && !hotkeyLatched) {
                hotkeyLatched = true;
                auto& mgr = TaskbarManager::getInstance();
                if (mgr.callSetting == 2) {
                    if (!mgr.isPaused()) {
                        lastMode = mgr.currentMode().load();
                        mgr.isPaused() = true;
                        mgr.ControlTaskbarLock(MODE_ALWAYS_SHOW);
                    }
                    else {
                        mgr.isPaused() = false;
                        mgr.ControlTaskbarLock(lastMode);
                    }
                }
            }
            else if (!hotkeyPressed) {
                hotkeyLatched = false;
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// 刷新任务栏信息(句柄，矩形)
void TaskbarManager::RefreshTaskbarInfo() {
    if (!g_hMainTaskbar || !IsWindow(g_hMainTaskbar)) {
        g_hMainTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    }
    APPBARDATA abd = {sizeof(APPBARDATA), g_hMainTaskbar};
    if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
        g_taskbarRect = abd.rc;
    }
}

bool TaskbarManager::IsExcludedWindow(HWND hwnd) {
    if (!hwnd)
        return true;
    wchar_t cls[256];
    if (!GetClassNameW(hwnd, cls, 256))
        return false;
    std::wstring cn(cls);
    return (cn == L"Shell_TrayWnd" || cn == L"SecondaryTrayWnd" ||
            cn == L"WorkerW" || cn == L"Progman" ||
            cn == L"Windows.UI.Core.CoreWindow" || cn == L"Shell_Context");
}

void TaskbarManager::ControlTaskbarLock(TaskbarMode mode) {
    //std::cout << "adjust taskbar to: " << mode << std::endl;
    switch (mode) {
        case MODE_ALWAYS_SHOW:
            if (AutoHideTag == 0 && this->windowStatus != 2) {
                SetTaskbarAutoHide(false);
            }
            ShowTaskbar1(true);
            ShowTaskbar2(true);
            break;
        case MODE_AUTO_HIDE_SOFT:
            // 在非全屏时，窗口底部触碰到任务栏时，不调整任务栏样式，只透明处理，防止窗口抖动
            if (AutoHideTag == 0) {
                if (this->windowStatus != 2) {
                    SetTaskbarAutoHide(true);
                }
                else {
                    ShowTaskbar1(false);
                }
            }
            else {
                ShowTaskbar1(true);
            }
            ShowTaskbar2(false);
            break;
        case MODE_AUTO_HIDE_LOCKED:
            if (AutoHideTag == 0 && this->windowStatus != 2) {
                SetTaskbarAutoHide(false);
            }
            ShowTaskbar1(false);
            ShowTaskbar2(false);
            break;
    }
    g_currentMode = mode;
}

bool TaskbarManager::CanAdjustTaskbar() {
    if (g_isPaused)
        return false;
    return true;
}

// 窗口的状态
void TaskbarManager::UpDateWindowStatus() {
    // 0: 在桌面 1：全屏 2：窗口底部触碰到任务栏
    int status = 0;
    struct Context {
        TaskbarManager *ptr;
        int *res;
    };
    Context ctx = {this, &status};
    EnumWindows(
        [](HWND hwnd, LPARAM lp) -> BOOL {
            Context *c = (Context *) lp;
            if (!IsWindowVisible(hwnd) || IsIconic(hwnd) ||
                c->ptr->IsExcludedWindow(hwnd)) {
                return TRUE;
            }
            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            if (exStyle & WS_EX_TOOLWINDOW)
                return TRUE;

            char title[256];
            if (GetWindowTextA(hwnd, title, sizeof(title)) == 0) {
                return TRUE;
            }
            // 判断是不是主显示器所在窗口
            HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
            if (hMonitor) {
                MONITORINFO mi;
                mi.cbSize = sizeof(MONITORINFO);
                if (GetMonitorInfo(hMonitor, &mi)) {
                    if (!(mi.dwFlags & MONITORINFOF_PRIMARY)) {
                        return TRUE;
                    }
                }
            }
            RECT wr;
            if (GetWindowRect(hwnd, &wr)) {
                //std::cout << "wr.bottom: " << wr.bottom << ", g_taskbarRect.top: " << c->ptr->g_taskbarRect.top << std::endl;
                if (wr.bottom >= c->ptr->g_taskbarRect.top) {
                    if (wr.top <= 0) {
                        *c->res = 1;
                    }
                    else {
                        *c->res = 2;
                    }
                    return FALSE;
                }
            }
            return TRUE;
        },
        (LPARAM) &ctx);
    //std::cout << "window status: " << status << std::endl;
    this->windowStatus = status;
}

// 判断应该调整为哪种模式
TaskbarMode TaskbarManager::ShouldTaskbarHide() {
    UpDateWindowStatus();
    switch (this->windowStatus) {
        case 0:
            return (TaskbarMode) ((ModeSetting >> 4) & 3);
        case 1:
            return (TaskbarMode) ((ModeSetting >> 2) & 3);
        case 2:
            return (TaskbarMode) (ModeSetting & 3);
        default:
            return g_currentMode;
    }
}

// 设置任务栏自动隐藏(系统设置)
void TaskbarManager::SetTaskbarAutoHide(bool enable) {
    APPBARDATA abd = {0};
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = g_hMainTaskbar;
    if (abd.hWnd == nullptr)
        return;
    abd.lParam = enable ? ABS_AUTOHIDE : ABS_ALWAYSONTOP;
    SHAppBarMessage(ABM_SETSTATE, &abd);
}

// 直接隐藏任务栏，就是简单将任务栏所在区域透明
void TaskbarManager::ShowTaskbar1(bool isShow) {
    if (isShow) {
        ShowWindow(this->g_hMainTaskbar, SW_SHOW);
    }
    else {
        ShowWindow(this->g_hMainTaskbar, SW_HIDE);
    }
}

// 仅适合自动隐藏模式下，包含过渡动画
void TaskbarManager::ShowTaskbar2(bool isShow) {
    HMONITOR hMon =
            MonitorFromWindow(this->g_hMainTaskbar, MONITOR_DEFAULTTONEAREST);
    if (isShow)
        PostMessage(this->g_hMainTaskbar, TRAY_BAR_FLAG, (WPARAM) 1, (LPARAM) hMon);
    else {
        PostMessage(this->g_hMainTaskbar, TRAY_BAR_FLAG, (WPARAM) 0, (LPARAM) hMon);
    }
}

// 注入/卸载 DLL 到资源管理器，拦截它对任务栏的控制
void TaskbarManager::injector(bool isInject) {
    g_hMainTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!g_hMainTaskbar) {
        if (isInject) g_isInjected = false;
        return;
    }
    DWORD pid;
    GetWindowThreadProcessId(g_hMainTaskbar, &pid);

    HMODULE existingModule = GetRemoteModuleHandle(pid, L"AutoTaskbarHook.dll");
    if (isInject && existingModule) {
        g_isInjected = true;
        return;
    }

    // 进程句柄
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        if (isInject) g_isInjected = false;
        return;
    }

    if (isInject) {
        // 注入 ---
        wchar_t currentDir[MAX_PATH];
        GetModuleFileNameW(nullptr, currentDir, MAX_PATH);
        PathRemoveFileSpecW(currentDir);

        //  DLL路径
        std::wstring fullDllPath =
                std::wstring(currentDir) + L"\\AutoTaskbarHook.dll";
        const wchar_t *dllPath = fullDllPath.c_str();
        if (_waccess(dllPath, 0) != 0) {
            // std::wcerr << L"ERROR: DLL file not found at: " << dllPath << std::endl;
            g_isInjected = false;
            CloseHandle(hProcess);
            return;
        }

        size_t pathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
        LPVOID pBuf =
                VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT, PAGE_READWRITE);
        if (pBuf) {
            WriteProcessMemory(hProcess, pBuf, dllPath, pathSize, nullptr);
            LPVOID pLoadLibrary =
                    GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
            HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                                (LPTHREAD_START_ROUTINE) pLoadLibrary,
                                                pBuf, 0, nullptr);
            if (hThread) {
                WaitForSingleObject(hThread, INFINITE);
                CloseHandle(hThread);
                g_isInjected = GetRemoteModuleHandle(pid, L"AutoTaskbarHook.dll") != nullptr;
                //std::cout << "SUCCESS: DLL Injected and Hooked." << std::endl;
            }
            VirtualFreeEx(hProcess, pBuf, 0, MEM_RELEASE);
        }
    }
    else {
        // 卸载
        HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (!hTray) {
            //std::cout << "Error: Could not find Shell_TrayWnd" << std::endl;
            return;
        }

        GetWindowThreadProcessId(hTray, &pid);
        HMODULE hModInRemote = GetRemoteModuleHandle(pid, L"AutoTaskbarHook.dll");
        if (hModInRemote) {
            LPVOID pFreeLibrary =
                    GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary");
            HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                                (LPTHREAD_START_ROUTINE) pFreeLibrary,
                                                hModInRemote, 0, nullptr);
            if (hThread) {
                WaitForSingleObject(hThread, INFINITE);
                CloseHandle(hThread);
                g_isInjected = GetRemoteModuleHandle(pid, L"AutoTaskbarHook.dll") != nullptr;
                //std::cout << "SUCCESS: DLL Unloaded." << std::endl;
            }
        }
        else {
            g_isInjected = false;
            //std::cout << "WARNING: DLL Not Found in Remote Process." << std::endl;
        }
    }
    CloseHandle(hProcess);
}


TaskbarManager::TaskbarManager() {
    Init();
    injector(true);

    // 安装钩子
    InstallHook();
}

TaskbarManager::~TaskbarManager() {
    this->injector(false);
    ControlTaskbarLock(MODE_AUTO_HIDE_SOFT);
    ShowTaskbar2(false);

    // 卸载钩子
    if (this->m_hKeyHook) {
        UnhookWindowsHookEx(this->m_hKeyHook);
        this->m_hKeyHook = nullptr;
    }
    if (this->m_hMouseHook) {
        UnhookWindowsHookEx(this->m_hMouseHook);
        this->m_hMouseHook = nullptr;
    }
}


void TaskbarManager::Init() {
    if (!std::filesystem::exists(GetConfigPath())) {
        SaveConfig();
    }
    LoadConfig();
    //std::cout << "Init TaskbarManager: " << ModeSetting << " " << callSetting << std::endl;
    RefreshTaskbarInfo();
    // 自动隐藏系统设置开关
    if (((TaskbarMode) (ModeSetting >> 2) & 3) == MODE_ALWAYS_SHOW) {
        SetTaskbarAutoHide(false);
        AutoHideTag = 0;
    }
    else {
        SetTaskbarAutoHide(true);
        AutoHideTag = 1;
    }
    /* 重新安装钩子
    if (this->m_hKeyHook) {
        UnhookWindowsHookEx(this->m_hKeyHook);
        this->m_hKeyHook = nullptr;
    }
    if (this->m_hMouseHook) {
        UnhookWindowsHookEx(this->m_hMouseHook);
        this->m_hMouseHook = nullptr;
    }
    PostThreadMessage(GetThreadId(m_hookThread.native_handle()), WM_QUIT, 0, 0);
    InstallHook();
    */
}

void TaskbarManager::InstallHook() {
    m_hookThread = std::thread([this]() {
       // if (callSetting == 2) {
       //     // 键盘钩子
       //     this->m_hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL,KeyboardProc,GetModuleHandle(nullptr),0);
       // }
       // else if (callSetting == 1) {
       //     // 鼠标钩子
       //     this->m_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL,MouseProc,GetModuleHandle(nullptr),0);
       // }

       this->m_hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL,KeyboardProc,GetModuleHandle(nullptr),0);
       this->m_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL,MouseProc,GetModuleHandle(nullptr),0);
       // if (!this->m_hKeyHook && !this->m_hMouseHook) {
       //     return;
       // }
       MSG msg;
       while (GetMessage(&msg, nullptr, 0, 0)) {
           TranslateMessage(&msg);
           DispatchMessage(&msg);
       }
    });
    m_hookThread.detach();
}
