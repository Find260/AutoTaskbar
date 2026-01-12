#include "TaskbarManager.h"
#include "SystemTray.h"

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    auto& mgr = TaskbarManager::getInstance();
    if (nCode >= 0) {
        if (wParam == WM_LBUTTONDOWN) {
            MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
            POINT pt = pMouseStruct->pt;
            if (pt.y >= mgr.taskbarRect().bottom - 2) {
                mgr.isPaused() = true;
                mgr.ControlTaskbarLock(MODE_AUTO_HIDE_SOFT);
                std::thread([&mgr]() {
                    while (true) {
                        if (mgr.IsClickOutsideTaskbar()) {
                            mgr.isPaused() = false;
                            break;
                        }
                        Sleep(50);
                    }
                    mgr.ControlTaskbarLock(MODE_AUTO_HIDE_LOCKED);
                }).detach();
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main() {
    // 防止多个实例运行
    HANDLE hMutex = CreateMutex(NULL, TRUE, L"AutoTaskbar_Instance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 1;
    }
    // 开机延迟启动，避免与其他应用冲突
    if (wcsstr(GetCommandLineW(), L"-delay")) {
        Sleep(10000);
    }

    SetProcessDPIAware();
    auto& mgr = TaskbarManager::getInstance();

    // 初始化
    mgr.RefreshTaskbarInfo();
    SystemTray::getInstance().CreateTray(GetModuleHandle(NULL));

    // 鼠标钩子线程
    std::thread([]() {
        HHOOK hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        UnhookWindowsHookEx(hMouseHook);
    }).detach();

    if (mgr.CanAdjustTaskbar()) {
        TaskbarMode initialMode = mgr.ShouldTaskbarHide();
        mgr.ControlTaskbarLock(initialMode);
       // std::cout << "initial taskbar lock mode: " << initialMode << std::endl;
    }

    int checkCounter = 0;
    while (true) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                // 释放互斥体
                ReleaseMutex(hMutex);
                CloseHandle(hMutex);
                SystemTray::getInstance().RemoveTray();
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (mgr.CanAdjustTaskbar()) {
            TaskbarMode targetMode = mgr.ShouldTaskbarHide();
            //std::cout << "should hide: " << targetMode << ", current: " << mgr.currentMode() << std::endl;
            if (targetMode != mgr.currentMode() || checkCounter % 10 == 0) {
                checkCounter = 0;
               // std::cout << "adjust taskbar lock to: " << targetMode << std::endl;
                mgr.RefreshTaskbarInfo();
                mgr.ControlTaskbarLock(targetMode);
            }
        }
        checkCounter++;
        Sleep(300);
    }
}
