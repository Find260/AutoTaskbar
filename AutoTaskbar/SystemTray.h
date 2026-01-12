#pragma once
#include <windows.h>

#define WM_TRAYICON (WM_USER + 100)
#define IDM_EXIT 1001
#define IDM_AUTORUN 1002
#define IDI_ICON1 101

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


private:
    SystemTray() : m_hwnd(NULL), m_hInstance(NULL) {
        memset(&m_nid, 0, sizeof(m_nid));
    }
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    NOTIFYICONDATAW m_nid;
    static UINT s_wmTaskbarCreated;
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};