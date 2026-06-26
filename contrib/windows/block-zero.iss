; Inno Setup script for the Block Zero wallet (Windows).
; Produces a single double-click installer that installs the GUI wallet
; per-user (no admin / UAC prompt) and creates Start Menu + Desktop shortcuts
; named "Block Zero".
;
; The source directory must already contain the deployed wallet:
;   - "Block Zero.exe" (the GUI), bitcoind.exe, bitcoin-cli.exe, ...
;   - the full Qt runtime (Qt6*.dll plus the platforms/ tls/ ... plugin folders)
;   - the VC++ runtime DLLs (msvcp140.dll, vcruntime140*.dll, ...)
; i.e. exactly what the release "Package" step / build-windows-installer.ps1
; assemble.
;
; Build:
;   ISCC.exe /DSourceDir="...\bin" /DAppVersion=1.0.0 contrib\windows\block-zero.iss

#ifndef SourceDir
  #error SourceDir must be defined (ISCC /DSourceDir=...)
#endif
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define AppName "Block Zero"
#define AppExe "Block Zero.exe"
#define AppPublisher "Block Zero"
#define AppURL "https://bloz.org"

[Setup]
AppId={{8F3A1C2E-5B6D-4E7F-9A0B-1C2D3E4F5A6B}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
; Per-user install: no administrator rights, no UAC prompt.
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=Block-Zero-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; Ship the entire deployed wallet folder. bitcoin-qt.exe is excluded in case a
; non-renamed copy is present; the GUI is shipped as "Block Zero.exe".
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "vc_redist.x64.exe,bitcoin-qt.exe"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
