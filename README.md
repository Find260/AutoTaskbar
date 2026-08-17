# AutoTaskbar

AutoTaskbar 是一款适用于 Windows 的任务栏自动管理工具。它可以根据当前窗口状态自动切换任务栏显示模式，并通过系统托盘提供快捷、直观的设置入口。

## 获取安装包

当前未使用 GitHub Releases 发布，安装包直接存放在仓库的 `release/` 目录中：

- `release/AutoTaskbar-v1.1.0-win-x64.zip`

压缩包内包含：

- `AutoTaskbar.exe` — 主程序
- `AutoTaskbarHook.dll` — Hook DLL

下载后将压缩包解压到任意目录，保持 `AutoTaskbar.exe` 与 `AutoTaskbarHook.dll` 位于同一目录，双击 `AutoTaskbar.exe` 即可运行，无需安装。

## 功能特性

- 根据使用场景自动管理任务栏：
  - 显示桌面时
  - 窗口最大化或全屏时
  - 普通窗口接触任务栏时
- 每个场景可独立设置为：
  - 始终显示：任务栏保持常驻可见状态，不随窗口变化而自动隐藏。
  - 自动隐藏：此模式与 Windows 原生自动隐藏行为一致，任务栏在不使用时自动滑入屏幕边缘隐藏；当鼠标光标移动至任务栏所在屏幕边缘时，任务栏自动滑出恢复显示，可在节省屏幕空间的同时保持快速访问能力。
  - 完全隐藏：任务栏彻底隐藏，且不会通过鼠标悬停自动呼出。用户需通过预设的呼出方式（禁止呼出、点击屏幕底部、自定义快捷键）才能临时恢复任务栏显示。该模式适用于追求极致沉浸感的场景，可有效避免鼠标误触导致的任务栏弹出干扰。
- 支持多种任务栏临时呼出方式：
  - 禁止呼出
  - 点击屏幕底部呼出
  - 使用自定义快捷键呼出
- 自定义快捷键支持 1 至 3 个按键组合。
- 防止与其它任务栏控制程序冲突，支持设置启动后的延迟注入时间。
- 托盘菜单实时显示倒计时和注入状态。
- 支持开机自启。
- 支持单实例运行。
- Explorer 重启后可自动恢复托盘图标和任务栏控制。

## 系统要求

- Windows 10 或 Windows 11
- x64 系统
- CMake 3.30 或更高版本
- Visual Studio 2022 或更高版本
- MSVC C++ 桌面开发工具和 Windows SDK

> 主程序和 Hook DLL 必须使用相同的 x64 架构进行编译。

## 项目结构

```text
AutoTaskbar/
├── AutoTaskbar/                 # 主程序
│   ├── main.cpp                 # 程序入口
│   ├── TaskbarManager.cpp       # 任务栏管理
│   ├── TaskbarManager.h
│   ├── SystemTray.cpp           # 托盘菜单和设置界面
│   ├── SystemTray.h
│   ├── resources.rc             # Windows 资源配置
│   ├── icon_256x256.ico
│   ├── icon_16x16.ico
│   └── CMakeLists.txt
├── AutoTaskbarHook/             # Hook DLL
│   ├── main.cpp
│   ├── detours.h
│   ├── lib/
│   └── CMakeLists.txt
├── .gitignore
├── release/                     # 安装包（AutoTaskbar-v1.1.0-win-x64.zip）
└── README.md
```

## 构建

### 1. 准备环境

安装 Visual Studio 2022，并在 Visual Studio Installer 中选择：

- 使用 C++ 的桌面开发
- MSVC x64/x86 生成工具
- Windows 10 或 Windows 11 SDK
- CMake tools for Windows

在仓库根目录打开 PowerShell，然后按照以下顺序构建。

### 2. 构建 Hook DLL

```powershell
cmake -S AutoTaskbarHook -B build/hook -G "Visual Studio 17 2022" -A x64
cmake --build build/hook --config Release --target AutoTaskbarHook
```

### 3. 构建主程序

```powershell
cmake -S AutoTaskbar -B build/app -G "Visual Studio 17 2022" -A x64
cmake --build build/app --config Release --target AutoTaskbar
```

### 4. 整理运行文件

将 Hook DLL 复制到主程序输出目录：

```powershell
Copy-Item build/hook/Release/AutoTaskbarHook.dll `
    build/app/Release/AutoTaskbarHook.dll -Force
```

完成后，运行目录应包含以下文件：

```text
build/app/Release/
├── AutoTaskbar.exe
└── AutoTaskbarHook.dll
```

### 使用 Visual Studio 2026

如果使用 Visual Studio 2026，请将生成器替换为：

```powershell
-G "Visual Studio 18 2026"
```

## 使用方法

### 启动

确保 `AutoTaskbar.exe` 与 `AutoTaskbarHook.dll` 位于同一目录，然后运行：

```powershell
./build/app/Release/AutoTaskbar.exe
```

程序启动后会驻留在系统托盘中，不会显示主窗口。单击或右键托盘图标即可打开设置菜单。

### 设置任务栏模式

托盘菜单提供以下场景设置：

- 在桌面时
- 最大化或全屏时
- 窗口碰到任务栏时

每个场景均可选择“显示”“自动隐藏”或“完全隐藏”。

### 设置任务栏呼出方式

在“呼出任务栏”菜单中选择所需方式：

- 永不呼出
- 底部左键
- 自定义快捷键

选择快捷键方式后，在弹出的设置窗口中按下 1 至 3 个按键，然后保存。

### 设置延迟注入

在托盘菜单中选择“延迟注入等待”，输入大于等于 0 的整数秒数：

- `0`：启动后立即执行任务栏控制。
- 大于 `0`：先显示托盘图标，倒计时结束后再执行任务栏控制。

设置将在下次启动时生效。等待期间，托盘菜单会实时显示剩余时间；完成后会显示当前注入状态。

### 设置开机自启

点击托盘菜单中的“开机自启”即可启用或关闭。菜单项带有勾选标记时表示已启用。

### 退出

点击托盘菜单中的“退出程序”即可关闭 AutoTaskbar。

## 构建输出清理

如需重新生成完整构建目录，可以删除 `build` 后重新执行构建命令：

```powershell
Remove-Item build -Recurse -Force
```

