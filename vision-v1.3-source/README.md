# vision v1.3

Windows 11 x64 本地视频播放器，C++20 + Qt Widgets，点缀色 **#005fff**。

当前为 v1.3 手动验收版本：已接入 libmpv，包含本地视频播放、真实逐帧、倍速、音轨字幕、同步、截图、窗口模式、历史收藏与离线安装器。左右键按实际展示帧前后步进，不用固定时间跳转模拟。完整视觉、真实音频设备、硬解性能和安装过程由用户手动验收。

## 结构

```text
cmake/                 生成版本常量
src/app/               应用入口
src/ui/                最小 MainWindow；后续窗口与界面
src/player/            libmpv 控制、事件与逐帧队列
src/models/            列表职责说明（v1.0 由界面列表维护）
src/storage/           SQLite / JSON 本地存储
resources/windows/     Windows 程序版本资源
scripts/build.ps1      后台编译，不启动程序
tests/                 分阶段验证计划
packaging/             安装包规划
docs/                  依赖、阶段验收记录
.deps/                 本机 Qt SDK（不进入源码包）
.tools/                本机依赖获取工具（不进入源码包）
build/                 编译输出（不进入源码包）
```

## 构建

需要 MSVC 2022 x64、Windows SDK、CMake ≥3.24、Ninja 和 Qt **6.8.3 msvc2022_64**（qtbase）。当前脚本默认使用本机 `C:\BuildTools2022`，也可显式指定已安装工具位置。

```powershell
# 在项目根目录运行；默认 Qt 路径 .deps/Qt/6.8.3/msvc2022_64
./scripts/build.ps1

# 其他开发机
./scripts/build.ps1 -QtRoot 'D:\Qt\6.8.3\msvc2022_64' -VisualStudioRoot 'D:\BuildTools2022'
```

另外需要 `.deps/mpv/include/mpv/client.h` 与 `.deps/mpv/libmpv-2.dll`。使用的精确构建及来源见 licenses/THIRD-PARTY-NOTICES.md；下载指定 LGPL 开发包后解压至 .deps/mpv。

输出 `build/windows-release/vision.exe`，最多 2 个并发编译任务。开发产物不能单独拷到其他电脑使用；请使用 `dist/vision-v1.1-setup-x64.exe`。构建脚本不启动 GUI、不注册关联、不修改系统 PATH。`scripts/package.ps1` 使用便携 NSIS、app-local Qt/MSVC/Vulkan 组件生成安装器。

Qt 获取记录见 docs/dependencies.md，第二阶段文档保留为历史记录，最终测试记录见 docs/release-v1.0.md。下一次对外交付按 v1.1 迭代。使用方法见 docs/使用与测试说明.md。
