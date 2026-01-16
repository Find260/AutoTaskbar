#pragma once
#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
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

HMODULE GetRemoteModuleHandle(DWORD pid, const wchar_t *moduleName);

class TaskbarManager {
public:
    static TaskbarManager &getInstance() {
        static TaskbarManager instance;
        return instance;
    }

    int AutoHideTag;

    void Init();

    void SetTaskbarAutoHide(bool enable);

    void ShowTaskbar1(bool isShow);

    void ShowTaskbar2(bool isShow);

    void RefreshTaskbarInfo();

    void injector(bool tag);

    bool IsExcludedWindow(HWND hwnd);

    void ControlTaskbarLock(TaskbarMode mode);

    bool CanAdjustTaskbar();

    TaskbarMode ShouldTaskbarHide();

    std::atomic<TaskbarMode> &currentMode() { return g_currentMode; }

    std::atomic<bool> &isPaused() { return g_isPaused; }

    RECT &taskbarRect() { return g_taskbarRect; }

//00显示、01自动隐藏、10完全隐藏， 按桌面，全屏，与任务栏碰撞排序
    static int ModeSetting;
    static int callSetting;
private:
    TaskbarManager();

    ~TaskbarManager();

    std::atomic<TaskbarMode> g_currentMode{MODE_ALWAYS_SHOW};
    std::atomic<bool> g_isPaused{false};

    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static void LoadConfig();
    static void SaveConfig();
    static int ParseJsonValue(const std::wstring& content, const std::wstring& key);
    static std::wstring GetConfigPath();

    HHOOK m_hMouseHook = nullptr;
    std::thread m_hookThread;
    std::thread m_loopThread;
    std::atomic<bool> m_stopRequested{false};
    RECT g_taskbarRect = {0};
    HWND g_hMainTaskbar = NULL;
    int windowStatus;
    void UpDateWindowStatus();
};