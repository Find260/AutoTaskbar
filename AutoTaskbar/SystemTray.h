#pragma once
#include <windows.h>
#include <string>
#include <fstream>
#include <array>
#include "TaskbarManager.h"

#define WM_TRAYICON (WM_USER + 100)
#define IDI_ICON1 101

// 菜单基础 ID
#define IDM_EXIT 1001
#define IDM_AUTORUN 1002

// 场景 ID 偏移量
#define IDM_SCENE_DESKTOP 1100
#define IDM_SCENE_MAXIMIZED 1200
#define IDM_SCENE_TOUCH 1300
#define IDM_SCENE_CALL 1400
#define IDM_INJECTION_DELAY 1500
#define IDM_INJECTION_STATUS 1501
#define IDT_INJECTION_STATUS 1

#define IDC_HOTKEY_DISPLAY 2001
#define IDC_HOTKEY_SAVE 2002
#define IDC_HOTKEY_CLEAR 2003
#define IDC_HOTKEY_CANCEL 2004
#define IDC_DELAY_EDIT 2101
#define IDC_DELAY_SAVE 2102
#define IDC_DELAY_CANCEL 2103

class SystemTray {
public:
    static SystemTray& getInstance() {
        static SystemTray instance;
        return instance;
    }

    void CreateTray(HINSTANCE hInstance);
    void RemoveTray();
    void SetAutoStart(bool enable);
    bool IsAutoStartEnabled();
    void ReAddTray();

    void SaveConfig();
    void LoadConfig();

    // UI 层状态变量
    int m_desktopMode;
    int m_maxMode;
    int m_touchMode;
    int m_callMode;
    int m_injectionDelaySeconds;
    std::wstring m_hotkeyStr;
    std::array<DWORD, 3> m_hotkeyKeys;

    void InitializeTaskbarManager();
    bool IsTaskbarManagerReady() const { return m_taskbarManagerReady; }
    void SetInjectionDelayStatus(int remainingSeconds);

private:
    SystemTray() : m_hwnd(NULL), m_hInstance(NULL),
                   m_desktopMode(0), m_maxMode(0),
                   m_touchMode(0), m_callMode(0), m_injectionDelaySeconds(60),
                   m_hotkeyStr(L"Win"), m_hotkeyKeys{VK_LWIN, 0, 0} {
        memset(&m_nid, 0, sizeof(m_nid));
    }

    HWND m_hwnd;
    HINSTANCE m_hInstance;
    NOTIFYICONDATAW m_nid;
    bool m_taskbarManagerReady = false;
    HMENU m_activeMenu = nullptr;
    int m_injectionRemainingSeconds = 0;
    ULONGLONG m_injectionStatusUpdatedAt = 0;
    static UINT s_wmTaskbarCreated;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK HotkeyWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK InjectionDelayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void ShowContextMenu(HWND hWnd);
    bool ShowHotkeyDialog(HWND owner);
    bool ShowInjectionDelayDialog(HWND owner);
    int GetCurrentInjectionRemainingSeconds() const;
    std::wstring GetInjectionStatusText() const;
    void UpdateInjectionStatusMenu();
    static DWORD NormalizeHotkey(DWORD vkCode);
    static std::wstring GetKeyName(DWORD vkCode);
    static std::wstring FormatHotkey(const std::array<DWORD, 3>& keys);
    std::wstring GetConfigPath();
};

//atb-tmp