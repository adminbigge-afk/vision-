# 第二阶段验收记录

日期：2026-09-09。状态：项目骨架创建完成，Windows x64 Release 编译和链接通过。

## 交付范围

- C++20 / Qt Widgets / CMake 工程，已有应用入口与最小 MainWindow。
- 项目命名 vision、版本 v1.0（开发中），蓝色点缀常量 #005fff。
- 播放、模型、存储、测试和打包目录已建立；未提前写入模拟播放实现。
- 构建脚本复用现有 MSVC 与 SDK，仅修改进程环境，退出恢复；编译并发上限 2。
- Qt 6.8.3 qtbase 与开发期获取工具隔离在项目目录，不进入源码压缩包。

## 实际验证

1. `scripts/build.ps1 -Fresh` 完成 CMake 配置、生成、6 个构建步骤以及链接，退出码 0。
2. MSVC 实际识别为 19.44.35228.0；使用 Windows SDK 10.0.26100.0。
3. 读取 vision.exe PE 头：Machine 0x8664（x64），Subsystem 2（Windows GUI）。
4. 读取版本资源：ProductName=vision，FileVersion/ProductVersion=1.0.0.0，OriginalFilename=vision.exe。
5. 静态依赖表包含 Qt6Widgets/Qt6Gui/Qt6Core 和 Windows/MSVC 运行库，尚未链接 mpv。

本机完整构建日志位于 `build/stage2-build.log`。配置时可选 Vulkan 头文件未找到，不影响当前 Qt Widgets 空窗口构建。已修正本机工具不在 PATH 时的编译器和 SDK 工具定位。

## 尚未验证与阶段边界

为避免影响用户操作，未启动 vision、未显示窗口、未播放媒体、未占用 GPU 做性能测试。编译成功不代表 GUI 视觉验收、逐帧精度或播放能力已通过。最小窗口只有基础调色板，完整专业播放器 UI 留在第四阶段。

libmpv 源码/API 基线指定为 v0.40.0，但可分发二进制、完整依赖和许可组合需在第三阶段实际接入前落实。SQLite 与 NSIS 尚未成为本阶段构建依赖。当前源码包不含 SDK、Qt DLL 或已部署可执行程序；正式离线安装包在第五阶段交付。

下一阶段：经用户确认后实现播放核心，包含左右方向键真实逐帧、帧率识别、轨道和同步。不能把本阶段产物当成可播放软件。
