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
#include <tlhelp32.h>
#include <Shlwapi.h>
#include <array>

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

    void InstallHook();

    bool IsExcludedWindow(HWND hwnd);

    void ControlTaskbarLock(TaskbarMode mode);

    bool CanAdjustTaskbar();

    TaskbarMode ShouldTaskbarHide();

    std::atomic<TaskbarMode> &currentMode() { return g_currentMode; }

    std::atomic<bool> &isPaused() { return g_isPaused; }

    std::atomic<bool> &isInjected() { return g_isInjected; }

    RECT &taskbarRect() { return g_taskbarRect; }

//00显示、01自动隐藏、10完全隐藏， 按桌面，全屏，与任务栏碰撞排序
    static int ModeSetting;
    static std::atomic<int> callSetting;
    static std::array<std::atomic<DWORD>, 3> HotkeyKeys;
    // 临时调整任务栏时，保存的原状态
    static TaskbarMode lastMode;
private:
    TaskbarManager();

    ~TaskbarManager();

    std::atomic<TaskbarMode> g_currentMode{MODE_ALWAYS_SHOW};
    std::atomic<bool> g_isPaused{false};
    std::atomic<bool> g_isInjected{false};

    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    static void LoadConfig();
    static void SaveConfig();
    static int ParseJsonValue(const std::wstring& content, const std::wstring& key);
    static DWORD NormalizeHotkey(DWORD vkCode);
    static bool IsHotkeyPressed(const std::array<bool, 256>& pressedKeys);
    static std::wstring GetConfigPath();

    HHOOK m_hMouseHook = nullptr;
    HHOOK m_hKeyHook = nullptr;
    std::thread m_hookThread;
    std::atomic<bool> m_stopRequested{false};
    RECT g_taskbarRect = {0};
    HWND g_hMainTaskbar = NULL;
    int windowStatus;
    void UpDateWindowStatus();
};
