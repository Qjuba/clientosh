; ============================================================
;  clientosh – NSIS Installer Script
;  Builds a single-file Windows installer (.exe)
; ============================================================

!define APP_NAME        "clientosh"
!define APP_VERSION     "1.0.0"
!define APP_PUBLISHER   "clientosh"
!define APP_URL         "https://github.com/clientosh/clientosh"
!define EXE_NAME        "clientosh.exe"
!define UNINSTALL_KEY   "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

; Output installer filename
OutFile "..\clientosh-${APP_VERSION}-setup.exe"

; Default install directory
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "${UNINSTALL_KEY}" "InstallLocation"

; Request administrator privileges
RequestExecutionLevel admin

; Modern UI
!include "MUI2.nsh"

; ---- UI pages ----
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language
!insertmacro MUI_LANGUAGE "English"

; ---- Branding ----
Name "${APP_NAME} ${APP_VERSION}"
BrandingText "${APP_PUBLISHER}"
Icon "..\packaging\clientosh.ico"
UninstallIcon "..\packaging\clientosh.ico"

; Use the app icon in the installer MUI pages (welcome/finish), and the same
; .ico in shortcuts / uninstaller entries at runtime.
!define MUI_ICON   "..\packaging\clientosh.ico"
!define MUI_UNICON "..\packaging\clientosh.ico"

; ---- Version info embedded in the installer EXE ----
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName"      "${APP_NAME}"
VIAddVersionKey "ProductVersion"   "${APP_VERSION}"
VIAddVersionKey "CompanyName"      "${APP_PUBLISHER}"
VIAddVersionKey "FileDescription"  "clientosh SSH Client Installer"
VIAddVersionKey "FileVersion"      "${APP_VERSION}"
VIAddVersionKey "LegalCopyright"   "MIT"

; ============================================================
;  Install section
; ============================================================
Section "Main" SecMain

  SetOutPath "$INSTDIR"

  ; Main executable
  File "..\build\${EXE_NAME}"

  ; Application icon (installed next to the exe for shortcuts / uninstaller)
  File "..\packaging\clientosh.ico"

  ; Qt & runtime DLLs (root of build dir)
  File "..\build\Qt6Core.dll"
  File "..\build\Qt6Gui.dll"
  File "..\build\Qt6Network.dll"
  File "..\build\Qt6OpenGL.dll"
  File "..\build\Qt6OpenGLWidgets.dll"
  File "..\build\Qt6Svg.dll"
  File "..\build\Qt6Widgets.dll"
  File "..\build\D3Dcompiler_47.dll"
  File "..\build\opengl32sw.dll"

  ; libssh + crypto DLLs
  File "..\build\libssh.dll"
  File "..\build\libcrypto-3-x64.dll"
  File "..\build\libssl-3-x64.dll"
  File "..\build\libgcc_s_seh-1.dll"
  File "..\build\libstdc++-6.dll"
  File "..\build\libwinpthread-1.dll"
  File "..\build\zlib1.dll"

  ; Qt platform plugin
  SetOutPath "$INSTDIR\platforms"
  File "..\build\platforms\qwindows.dll"

  ; Qt image format plugins
  SetOutPath "$INSTDIR\imageformats"
  File "..\build\imageformats\qgif.dll"
  File "..\build\imageformats\qicns.dll"
  File "..\build\imageformats\qico.dll"
  File "..\build\imageformats\qjpeg.dll"
  File "..\build\imageformats\qsvg.dll"
  File "..\build\imageformats\qtga.dll"
  File "..\build\imageformats\qtiff.dll"
  File "..\build\imageformats\qwbmp.dll"
  File "..\build\imageformats\qwebp.dll"

  ; Qt style plugin
  SetOutPath "$INSTDIR\styles"
  File "..\build\styles\qmodernwindowsstyle.dll"

  ; ---- Write uninstall registry keys ----
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "DisplayName"          "${APP_NAME} ${APP_VERSION}"
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "UninstallString"      "$INSTDIR\uninstall.exe"
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "InstallLocation"      "$INSTDIR"
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "DisplayIcon"          "$INSTDIR\clientosh.ico"
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "Publisher"            "${APP_PUBLISHER}"
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "URLInfoAbout"         "${APP_URL}"
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "DisplayVersion"       "${APP_VERSION}"
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify"             1
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair"             1

  ; Let Add/Remove programs display an accurate installed size
  Call EstimateInstallSize
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "EstimatedSize"        $0

  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; ---- Start Menu shortcut ----
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut  "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" \
                  "$INSTDIR\${EXE_NAME}" "" "$INSTDIR\clientosh.ico" 0
  CreateShortcut  "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" \
                  "$INSTDIR\uninstall.exe" "" "$INSTDIR\clientosh.ico" 0

  ; ---- Desktop shortcut ----
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" \
                 "$INSTDIR\${EXE_NAME}" "" "$INSTDIR\clientosh.ico" 0

SectionEnd

; ============================================================
;  Uninstall section
; ============================================================
Section "Uninstall"

  ; Remove files
  Delete "$INSTDIR\${EXE_NAME}"
  Delete "$INSTDIR\clientosh.ico"
  Delete "$INSTDIR\Qt6Core.dll"
  Delete "$INSTDIR\Qt6Gui.dll"
  Delete "$INSTDIR\Qt6Network.dll"
  Delete "$INSTDIR\Qt6OpenGL.dll"
  Delete "$INSTDIR\Qt6OpenGLWidgets.dll"
  Delete "$INSTDIR\Qt6Svg.dll"
  Delete "$INSTDIR\Qt6Widgets.dll"
  Delete "$INSTDIR\D3Dcompiler_47.dll"
  Delete "$INSTDIR\opengl32sw.dll"
  Delete "$INSTDIR\libssh.dll"
  Delete "$INSTDIR\libcrypto-3-x64.dll"
  Delete "$INSTDIR\libssl-3-x64.dll"
  Delete "$INSTDIR\libgcc_s_seh-1.dll"
  Delete "$INSTDIR\libstdc++-6.dll"
  Delete "$INSTDIR\libwinpthread-1.dll"
  Delete "$INSTDIR\zlib1.dll"
  Delete "$INSTDIR\uninstall.exe"

  ; Remove plugin subdirs
  Delete "$INSTDIR\platforms\qwindows.dll"
  RMDir  "$INSTDIR\platforms"
  Delete "$INSTDIR\imageformats\*.dll"
  RMDir  "$INSTDIR\imageformats"
  Delete "$INSTDIR\styles\*.dll"
  RMDir  "$INSTDIR\styles"

  RMDir  "$INSTDIR"

  ; Remove shortcuts
  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"
  RMDir  "$SMPROGRAMS\${APP_NAME}"
  Delete "$DESKTOP\${APP_NAME}.lnk"

  ; Remove registry keys
  DeleteRegKey HKLM "${UNINSTALL_KEY}"

SectionEnd

; ============================================================
;  Helper: compute installed size (KB) under $INSTDIR
;  for the Add/Remove Programs "EstimatedSize" value.
; ============================================================
Function EstimateInstallSize
  ; Returns installed size in KB in $0.
  Push $1   ; directory to scan
  Push $2   ; find handle
  Push $3   ; filename
  Push $4   ; temp file size
  StrCpy $0 0
  Push "$INSTDIR"
  dirloop:
    Pop $1
    StrCmp $1 "" alldone
    FindFirst $2 $3 "$1\*"
    findloop:
      StrCmp $3 "" finddone
      StrCmp $3 "." nextfile
      StrCmp $3 ".." nextfile
      IfFileExists "$1\$3\*.*" 0 isfile
        Push "$1\$3"
        Goto nextfile
      isfile:
        FileOpen $4 "$1\$3" r
        IfErrors nextfile
        FileSeek $4 0 END
        FileSeek $4 0 CUR $4
        IntOp $0 $0 + $4
        FileClose $4
      nextfile:
      FindNext $2 $3
      Goto findloop
    finddone:
    FindClose $2
    Goto dirloop
  alldone:
  IntOp $2 $0 % 1024
  IntOp $0 $0 / 1024
  IntCmp $2 0 skipround
  IntOp $0 $0 + 1
  skipround:
  Pop $4
  Pop $3
  Pop $2
  Pop $1
FunctionEnd
