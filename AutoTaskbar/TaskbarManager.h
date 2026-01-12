#pragma once
#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <iostream>
#include <io.h>
#include <tlhelp32.h>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

const UINT TRAY_BAR_FLAG = 0x05D1;
enum TaskbarMode {
    MODE_ALWAYS_SHOW = 0,
    MODE_AUTO_HIDE_SOFT = 1,
    MODE_AUTO_HIDE_LOCKED = 2
};
HMODULE GetRemoteModuleHandle(DWORD pid, const wchar_t* moduleName);

class TaskbarManager {
public:
    static TaskbarManager& getInstance() {
        static TaskbarManager instance;
        return instance;
    }
    void HideTaskbar();
    void ShowTaskbar();
    void RefreshTaskbarInfo();
    void injector(bool tag);
    std::vector<HWND> GetAllTrayWindows();
    bool IsMousePressed();
    bool IsClickOutsideTaskbar();
    bool IsExcludedWindow(HWND hwnd);
    void ControlTaskbarLock(TaskbarMode mode);
    bool CanAdjustTaskbar();
    TaskbarMode ShouldTaskbarHide();

    std::atomic<TaskbarMode>& currentMode() { return g_currentMode; }
    std::atomic<bool>& isPaused() { return g_isPaused; }
    RECT& taskbarRect() { return g_taskbarRect; }

private:
    TaskbarManager();
    ~TaskbarManager();
    std::atomic<TaskbarMode> g_currentMode{ MODE_ALWAYS_SHOW };
    std::atomic<bool> g_isPaused{ false };
    std::thread m_loopThread;
    std::atomic<bool> m_stopRequested{false};
    RECT g_taskbarRect = { 0 };
    HWND g_hMainTaskbar = NULL;
};