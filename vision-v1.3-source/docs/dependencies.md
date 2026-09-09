# 依赖基线

日期：2026-09-09。下表保留第二阶段构建基线，不声称使用各组件最新版。最终 v1.0 已接入 mpv v0.41.0-1042-g7e4cb538a（zhongfly LGPL 构建）、Qt Sql SQLite 驱动、NSIS 3.08 便携编译器和 Vulkan Loader 1.4.357.0；精确最终来源以 licenses/THIRD-PARTY-NOTICES.md 为准。

| 组件 | 基线 | 当前用途与状态 |
|---|---|---|
| C++ | C++20 | 实际源码标准 |
| MSVC | 2022 / 工具目录 14.44.35207 | 复用本机 BuildTools2022，编译器实际版本见阶段报告 |
| CMake | 3.31.6-msvc6 | 复用本机；项目最低 3.24 |
| Ninja | 随本机 BuildTools2022 | 不安装全局工具 |
| Qt | 6.8.3，win64_msvc2022_64，qtbase | 当前唯一直接运行时依赖；动态链接 Widgets/Core/Gui |
| aqtinstall | 3.3.0 | 仅开发期获取 Qt，安装在 .tools；不随播放器发布 |
| libmpv | 源码/API 基线 v0.40.0 | 第三阶段接入；当前未下载、未链接，不能视为已验证播放引擎 |
| SQLite | 后续采用 Qt SQL 的 SQLite 驱动 | 当前未链接；第四阶段记录驱动实际 SQLite 版本 |
| NSIS | 第五阶段锁定工具版本 | 当前未安装、未生成安装包 |

Qt 安装位置：`.deps/Qt/6.8.3/msvc2022_64`。通过 aqt 从 Qt 发布仓库下载 qtbase，工具校验下载档案；完整下载日志保留于本机 aqtinstall.log。发行前重新审查组件维护状态和依赖许可，必要时升级并重新验证。

获取命令（Python 应已安装 aqtinstall 3.3.0；本机使用 .tools/python-packages）：

```powershell
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -O .deps/Qt --archives qtbase
```

许可计划：本项目当前未确定对外源码许可证，不擅自给用户代码加 GPL 声明。Qt 开源模块的 LGPL/GPL/第三方条款需随实际使用模块保留；使用动态 Qt。libmpv 默认 GPL，闭源分发时需可追溯的 LGPL 构建及其依赖清单。当前仅锁定 libmpv 源码/API 版本，二进制供应链与最终发行许可组合尚未完成，不把未知 DLL 当作 LGPL。该项是第三阶段接入时的明确前置工作，当前 Qt 空窗口构建不依赖它。

来源：

- [Qt 6.8.3 发布档案](https://download.qt.io/archive/qt/6.8/6.8.3/)
- [Qt CMake 文档](https://doc.qt.io/qt-6/cmake-get-started.html)
- [aqtinstall 文档](https://aqtinstall.readthedocs.io/en/latest/getting_started.html)
- [mpv v0.40.0](https://github.com/mpv-player/mpv/releases/tag/v0.40.0)
- [mpv Copyright](https://github.com/mpv-player/mpv/blob/v0.40.0/Copyright)
