Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

!macro RegisterExtension EXT
  WriteRegStr HKCU "Software\Classes\Applications\vision.exe\SupportedTypes" "${EXT}" ""
  WriteRegStr HKCU "Software\vision\Capabilities\FileAssociations" "${EXT}" "vision.Video"
  WriteRegStr HKCU "Software\Classes\${EXT}\OpenWithProgids" "vision.Video" ""
!macroend

!macro UnregisterExtension EXT
  DeleteRegValue HKCU "Software\Classes\${EXT}\OpenWithProgids" "vision.Video"
!macroend

Name "vision v1.3"
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\vision"
InstallDirRegKey HKCU "Software\vision" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "vision · local video player"
VIProductVersion "1.3.0.0"
VIAddVersionKey /LANG=1033 "ProductName" "vision"
VIAddVersionKey /LANG=1033 "FileDescription" "vision v1.3 offline installer"
VIAddVersionKey /LANG=1033 "FileVersion" "1.3.0.0"
VIAddVersionKey /LANG=1033 "LegalCopyright" "vision"
!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION OpenDefaultApps
!define MUI_FINISHPAGE_RUN_TEXT "设为默认视频播放器"
!define MUI_FINISHPAGE_TEXT "vision 已安装完成。$\r$\n$\r$\n如勾选下方选项，点击完成后将打开 Windows 默认应用页面，请在那里确认视频文件关联。$\r$\n$\r$\n播放器不会自动启动。"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "vision 需要 64 位 Windows。"
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext current
  System::Call 'kernel32::OpenMutexW(i 0x00100000, i 0, w "Local\vision-running") p.r0'
  ${If} $0 P<> 0
    System::Call 'kernel32::CloseHandle(p r0)'
    MessageBox MB_ICONEXCLAMATION "请先关闭 vision，再安装或升级。"
    Abort
  ${EndIf}
FunctionEnd

Function OpenDefaultApps
  ClearErrors
  ExecShell "open" "ms-settings:defaultapps?registeredAppUser=vision"
  ${If} ${Errors}
    MessageBox MB_ICONINFORMATION "请在 Windows 设置 → 应用 → 默认应用中选择 vision，并设置视频文件关联。"
  ${EndIf}
FunctionEnd

Section "vision" SEC_MAIN
  SetOutPath "$INSTDIR"
  File /r "${STAGING}\*.*"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "Software\vision" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\vision" "DisplayName" "vision"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\vision" "DisplayVersion" "1.3"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\vision" "DisplayIcon" "$INSTDIR\vision.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\vision" "UninstallString" '$\"$INSTDIR\uninstall.exe$\"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\vision" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\vision" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\vision" "NoRepair" 1
  CreateDirectory "$SMPROGRAMS\vision"
  CreateShortcut "$SMPROGRAMS\vision\vision.lnk" "$INSTDIR\vision.exe"
  CreateShortcut "$SMPROGRAMS\vision\卸载 vision.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$DESKTOP\vision.lnk" "$INSTDIR\vision.exe"
  WriteRegStr HKCU "Software\Classes\Applications\vision.exe" "FriendlyAppName" "vision"
  WriteRegStr HKCU "Software\Classes\vision.Video" "" "vision Video"
  WriteRegStr HKCU "Software\Classes\vision.Video\DefaultIcon" "" '$\"$INSTDIR\vision.exe$\",0'
  WriteRegStr HKCU "Software\Classes\vision.Video\shell\open\command" "" '$\"$INSTDIR\vision.exe$\" $\"%1$\"'
  WriteRegStr HKCU "Software\vision\Capabilities" "ApplicationName" "vision"
  WriteRegStr HKCU "Software\vision\Capabilities" "ApplicationDescription" "vision 本地视频播放器"
  WriteRegStr HKCU "Software\vision\Capabilities" "ApplicationIcon" '$\"$INSTDIR\vision.exe$\",0'
  WriteRegStr HKCU "Software\RegisteredApplications" "vision" "Software\vision\Capabilities"
  WriteRegStr HKCU "Software\Classes\Applications\vision.exe\shell\open\command" "" '$\"$INSTDIR\vision.exe$\" $\"%1$\"'
  !insertmacro RegisterExtension ".mp4"
  !insertmacro RegisterExtension ".mkv"
  !insertmacro RegisterExtension ".avi"
  !insertmacro RegisterExtension ".mov"
  !insertmacro RegisterExtension ".webm"
  !insertmacro RegisterExtension ".flv"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

Function un.onInit
  SetRegView 64
  SetShellVarContext current
  System::Call 'kernel32::OpenMutexW(i 0x00100000, i 0, w "Local\vision-running") p.r0'
  ${If} $0 P<> 0
    System::Call 'kernel32::CloseHandle(p r0)'
    MessageBox MB_ICONEXCLAMATION "请先关闭 vision，再卸载。"
    Abort
  ${EndIf}
FunctionEnd

Section "Uninstall"
  !include "${UNINSTALL_FILES}"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\vision\vision.lnk"
  Delete "$SMPROGRAMS\vision\卸载 vision.lnk"
  RMDir "$SMPROGRAMS\vision"
  Delete "$DESKTOP\vision.lnk"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\vision"
  DeleteRegKey HKCU "Software\Classes\Applications\vision.exe"
  DeleteRegKey HKCU "Software\Classes\vision.Video"
  DeleteRegValue HKCU "Software\RegisteredApplications" "vision"
  !insertmacro UnregisterExtension ".mp4"
  !insertmacro UnregisterExtension ".mkv"
  !insertmacro UnregisterExtension ".avi"
  !insertmacro UnregisterExtension ".mov"
  !insertmacro UnregisterExtension ".webm"
  !insertmacro UnregisterExtension ".flv"
  DeleteRegKey HKCU "Software\vision"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
  ; Keep %LOCALAPPDATA%\vision user settings, favorites and history.
SectionEnd
