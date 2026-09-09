# vision v1.0 — third-party components

vision is an independent application. No PotPlayer binaries, skins or artwork are included.

- Qt 6.8.3: dynamically linked Core, Gui, Widgets and Sql, qwindows and qsqlite plugins. Qt LGPLv3/GPL alternatives and third-party notices are recorded in the accompanying license texts and qtbase SPDX document. Source: https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz
- libmpv: zhongfly LGPL x86_64 build, 2026-09-08-7e4cb538a3; runtime reports v0.41.0-1042-g7e4cb538a. Source commit: https://github.com/mpv-player/mpv/tree/7e4cb538a3f30d25920ad8e87ba6571540fb729f . Archive SHA256 was checked against the release publisher's sha256.txt. LGPLv2.1+ libmpv with statically linked LGPLv3 FFmpeg, as described by the build publisher. The earlier planned v0.40.0 baseline was replaced by this recorded binary build.
- libmpv binary and build details: https://github.com/zhongfly/mpv-winbuild/releases/tag/2026-09-08-7e4cb538a3 ; https://github.com/zhongfly/mpv-winbuild/actions/runs/34223679174 . Build recipe and LGPL patch are included as reference. FFmpeg source revision listed by the release: 1de77bb89, https://github.com/FFmpeg/FFmpeg/tree/1de77bb89 . Other transitive source repositories are listed in mpv-build-README.md and the upstream build recipe; this is a personal test package, not a claim of a completed commercial distribution audit.
- Vulkan Loader 1.4.357.0 x64: official LunarG runtime components, license included. Only the application-local loader DLL is shipped; the installer does not install or modify GPU drivers. Source: https://github.com/KhronosGroup/Vulkan-Loader/tree/v1.4.357 . Binary source: https://sdk.lunarg.com/sdk/download/1.4.357.0/windows/VulkanRT-X64-1.4.357.0-Components.zip
- Microsoft Visual C++ 2022 runtime: app-local redistributable DLLs from the installed Visual Studio Build Tools redistribution directory; Microsoft runtime terms apply.
- NSIS 3.08 builds this installer. It is a development tool, not installed on the target PC. Portable tool source: https://github.com/tauri-apps/binary-releases/releases/tag/nsis-3 ; NSIS project: https://nsis.sourceforge.io/ . NSIS license is included.

The application does not restrict modification or replacement of its LGPL libraries, or reverse engineering needed to debug such modifications. The separately delivered vision source archive includes the application sources and build instructions so it can be rebuilt against a compatible modified library. This notice does not assign a new license to the user's application code.

All runtime operation is local. Source links here are documentation only; the application does not fetch them or require a server.
