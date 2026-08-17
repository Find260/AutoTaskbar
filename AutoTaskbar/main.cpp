#include "SystemTray.h"

int main() {
    // 防止多个实例运行
    HANDLE hMutex = CreateMutex(NULL, TRUE, L"AutoTaskbar_Instance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 1;
    }

    //DPI缩放问题
    SetProcessDPIAware();

    auto& tray = SystemTray::getInstance();
    tray.CreateTray(GetModuleHandle(NULL));

    TaskbarManager* mgr = nullptr;
    bool waitingForInjection = tray.m_injectionDelaySeconds > 0;
    ULONGLONG injectionDeadline = GetTickCount64() +
            static_cast<ULONGLONG>(tray.m_injectionDelaySeconds) * 1000;
    int lastRemainingSeconds = -1;

    if (!waitingForInjection) {
        tray.InitializeTaskbarManager();
        mgr = &TaskbarManager::getInstance();
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

        if (waitingForInjection) {
            if (tray.IsTaskbarManagerReady()) {
                waitingForInjection = false;
                mgr = &TaskbarManager::getInstance();
            }
            ULONGLONG now = GetTickCount64();
            if (waitingForInjection && now >= injectionDeadline) {
                waitingForInjection = false;
                tray.InitializeTaskbarManager();
                mgr = &TaskbarManager::getInstance();
            }
            else if (waitingForInjection) {
                int remainingSeconds = static_cast<int>((injectionDeadline - now + 999) / 1000);
                if (remainingSeconds != lastRemainingSeconds) {
                    tray.SetInjectionDelayStatus(remainingSeconds);
                    lastRemainingSeconds = remainingSeconds;
                }
            }
        }

        if (mgr && mgr->CanAdjustTaskbar()) {
            TaskbarMode targetMode = mgr->ShouldTaskbarHide();
            if (targetMode != mgr->currentMode() || checkCounter % 10 == 0) {
                checkCounter = 0;
                mgr->RefreshTaskbarInfo();
                mgr->ControlTaskbarLock(targetMode);
            }
        }
        checkCounter++;
        Sleep(300);
    }
}

//atb-tmp