#include "TaskbarManager.h"

HMODULE GetRemoteModuleHandle(DWORD pid, const wchar_t* moduleName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnap == INVALID_HANDLE_VALUE) return NULL;

    MODULEENTRY32W me = { sizeof(me) };
    if (Module32FirstW(hSnap, &me)) {
        do {
            if (_wcsicmp(me.szModule, moduleName) == 0) {
                CloseHandle(hSnap);
                return (HMODULE)me.modBaseAddr;
            }
        } while (Module32NextW(hSnap, &me));
    }
    CloseHandle(hSnap);
    return NULL;
}

void TaskbarManager::RefreshTaskbarInfo() {
    if (!g_hMainTaskbar || !IsWindow(g_hMainTaskbar)) {
        g_hMainTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    }
    APPBARDATA abd = {sizeof(APPBARDATA), g_hMainTaskbar};
    if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
        g_taskbarRect = abd.rc;
    }
}

std::vector<HWND> TaskbarManager::GetAllTrayWindows() {
    std::vector<HWND> trays;
    if (g_hMainTaskbar) trays.push_back(g_hMainTaskbar);
    HWND hSec = NULL;
    while ((hSec = FindWindowExW(NULL, hSec, L"SecondaryTrayWnd", NULL))) {
        trays.push_back(hSec);
    }
    return trays;
}

bool TaskbarManager::IsMousePressed() {
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
}

bool TaskbarManager::IsClickOutsideTaskbar() {
    if (IsMousePressed()) {
        POINT pt;
        GetCursorPos(&pt);
        return !PtInRect(&g_taskbarRect, pt);
    }
    return false;
}

bool TaskbarManager::IsExcludedWindow(HWND hwnd) {
    if (!hwnd) return true;
    wchar_t cls[256];
    if (!GetClassNameW(hwnd, cls, 256)) return false;
    std::wstring cn(cls);
    return (cn == L"Shell_TrayWnd" || cn == L"SecondaryTrayWnd" || cn == L"WorkerW" ||
            cn == L"Progman" || cn == L"Windows.UI.Core.CoreWindow" || cn == L"Shell_Context");
}

void TaskbarManager::ControlTaskbarLock(TaskbarMode mode) {
    std::thread([this, mode]() {
        auto trays = GetAllTrayWindows();
        for (HWND hWnd: trays) {
            switch (mode) {
                case MODE_ALWAYS_SHOW:
                    ShowTaskbar();
                    break;
                case MODE_AUTO_HIDE_SOFT:
                    ShowTaskbar();
                    break;
                case MODE_AUTO_HIDE_LOCKED:
                    HideTaskbar();
                    break;
            }
        }
    }).detach();
    g_currentMode = mode;
}

bool TaskbarManager::CanAdjustTaskbar() {
    if (g_isPaused) return false;
    if (IsMousePressed()) return false;
    return true;
}

// 如果与任务栏没有碰撞则显示
TaskbarMode TaskbarManager::ShouldTaskbarHide() {
    bool collides = false;
    struct Context { TaskbarManager* ptr; bool* res; };
    Context ctx = { this, &collides };
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        Context* c = (Context*)lp;
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd) || c->ptr->IsExcludedWindow(hwnd)) {
            return TRUE;
        }
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

        char title[256];
        if (GetWindowTextA(hwnd, title, sizeof(title)) == 0) {
            return TRUE;
        }
        RECT wr;
        if (GetWindowRect(hwnd, &wr)) {
            if (wr.bottom > c->ptr->g_taskbarRect.top ||
                (wr.bottom == c->ptr->g_taskbarRect.top && wr.top <= 0)) {
                *c->res = true;
                return FALSE;
            }
        }
        return TRUE;
    }, (LPARAM)&ctx);
    return collides ? MODE_AUTO_HIDE_LOCKED : MODE_ALWAYS_SHOW;
}

void TaskbarManager::HideTaskbar() {
    if (this->g_hMainTaskbar == nullptr) return;
    BOOL ok = PostMessage(this->g_hMainTaskbar, TRAY_BAR_FLAG, (WPARAM) 0, (LPARAM) 0);
    ShowWindow(this->g_hMainTaskbar, SW_HIDE);
}

void TaskbarManager::ShowTaskbar(){
        if (this->g_hMainTaskbar == nullptr) return;
        HMONITOR hMon = MonitorFromWindow(this->g_hMainTaskbar, MONITOR_DEFAULTTONEAREST);
        BOOL ok = PostMessage(this->g_hMainTaskbar, TRAY_BAR_FLAG, (WPARAM) 1, (LPARAM) hMon);
        ShowWindow(this->g_hMainTaskbar, SW_SHOW);
}

void TaskbarManager::injector(bool tag) {
    DWORD pid;
    GetWindowThreadProcessId(g_hMainTaskbar, &pid);

    // 进程句柄
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        return;
    }

    if (tag) {
        // 注入 ---
        wchar_t currentDir[MAX_PATH];
        GetModuleFileNameW(NULL, currentDir, MAX_PATH);
        PathRemoveFileSpecW(currentDir); // 去掉 exe 文件名，保留目录

        // 2. 拼接成完整的 DLL 绝对路径
        std::wstring fullDllPath = std::wstring(currentDir) + L"\\AutoTaskbarHook.dll";
        const wchar_t* dllPath = fullDllPath.c_str();
        //路径检查
        if (_waccess(dllPath, 0) != 0) {
            std::wcerr << L"ERROR: DLL file not found at: " << dllPath << std::endl;
            return;
        }
        size_t pathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);

        LPVOID pBuf = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT, PAGE_READWRITE);
        if (pBuf) {
            WriteProcessMemory(hProcess, pBuf, dllPath, pathSize, NULL);
            LPVOID pLoadLibrary = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
            HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pBuf, 0, NULL);
            if (hThread) {
                WaitForSingleObject(hThread, INFINITE);
                CloseHandle(hThread);
                std::cout << "SUCCESS: DLL Injected and Hooked." << std::endl;
            }
        }
    } else {
        //卸载
        HWND hTray = FindWindowW(L"Shell_TrayWnd", NULL);
        if (!hTray) {
            std::cout << "Error: Could not find Shell_TrayWnd" << std::endl;
            return;
        }

        GetWindowThreadProcessId(hTray, &pid);
        HMODULE hModInRemote = GetRemoteModuleHandle(pid, L"AutoTaskbarHook.dll");
        if (hModInRemote) {
            LPVOID pFreeLibrary = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary");
            HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pFreeLibrary, hModInRemote, 0, NULL);
            if (hThread) {
                WaitForSingleObject(hThread, INFINITE);
                CloseHandle(hThread);
                std::cout << "SUCCESS: DLL Unloaded." << std::endl;
            }
        } else {
            std::cout << "WARNING: DLL Not Found in Remote Process." << std::endl;
        }
    }
    CloseHandle(hProcess);
}

TaskbarManager::TaskbarManager() {
    RefreshTaskbarInfo();
    m_loopThread = std::thread([this]() {
        while (!m_stopRequested) {
            this->injector(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        int i = 10;
        while (i--) {
            this->injector(false);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });
}

TaskbarManager::~TaskbarManager() {
    ShowWindow(this->g_hMainTaskbar, SW_SHOW);
    m_stopRequested = true;
    if (m_loopThread.joinable()) {
        m_loopThread.join();
    }
}
