#pragma once
#include <windows.h>
#include <string>
#include <fstream>
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
    std::wstring m_hotkeyStr;

private:
    SystemTray() : m_hwnd(NULL), m_hInstance(NULL),
                   m_desktopMode(0), m_maxMode(0),
                   m_touchMode(0), m_callMode(0),
                   m_hotkeyStr(L"Win") {
        memset(&m_nid, 0, sizeof(m_nid));
    }

    HWND m_hwnd;
    HINSTANCE m_hInstance;
    NOTIFYICONDATAW m_nid;
    static UINT s_wmTaskbarCreated;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    //static INT_PTR CALLBACK HotkeyDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

    void ShowContextMenu(HWND hWnd);
    std::wstring GetConfigPath();
};