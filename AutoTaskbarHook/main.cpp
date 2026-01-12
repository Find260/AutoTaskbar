#include <windows.h>
#include "detours.h"
#include <iostream>

#pragma comment(lib, "detours.lib")

// 1. 定义原始函数指针，初始化为指向真正的 API
static BOOL (WINAPI* TruePostMessageW)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) = PostMessageW;

// 2. 编写你自己的 Hook 函数 (对应 InjectionEntryPoint.cs 中的 PostMessageHook)
BOOL WINAPI MyPostMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    try {
        // 0x05D1 是特定的任务栏消息逻辑
        if (Msg == 0x05D1 && lParam == 0) {
            static HWND hTray = FindWindowW(L"Shell_TrayWnd", NULL);
            if (hWnd == hTray) {
                return FALSE; // 拦截消息，不让任务栏收到
            }
        }
    } catch (...) {
        return FALSE;
    }

    // 调用原始函数（Detours 会处理好 trampoline 跳回）
    return TruePostMessageW(hWnd, Msg, wParam, lParam);
}

// 3. 安装和卸载 Hook
void InstallHook() {
    DetourRestoreAfterWith(); // 恢复被修改的导入表（如果有）
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // 将 TruePostMessageW 替换为 MyPostMessageW
    DetourAttach(&(PVOID&)TruePostMessageW, MyPostMessageW);

    DetourTransactionCommit();
}

void UninstallHook() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // 移除 Hook
    DetourDetach(&(PVOID&)TruePostMessageW, MyPostMessageW);

    DetourTransactionCommit();
}

// DLL 入口点
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            InstallHook();
            break;
        case DLL_PROCESS_DETACH:
            UninstallHook();
            break;
    }
    return TRUE;
}