#define MyAppName "FundamentalUpdater_rev5"
#define MyAppVersion "5.0"
#define MyAppPublisher "Plaifa Engineering"
#define MyAppExeName "FundamentalUpdater_rev5.exe"

[Setup]
AppId={{8F7E4B4D-2C44-4F8D-9A9A-7C5F6E2D4A31}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\FundamentalUpdater_rev5
DefaultGroupName={#MyAppName}
OutputDir=.
OutputBaseFilename=FundamentalUpdater_rev5_Setup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "build-win-fixed\FundamentalUpdater_rev5.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build-win-fixed\FundamentalCapture.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build-win-fixed\FundamentalCsvConverter.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "symbols.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "config.ini"; DestDir: "{app}"; Flags: ignoreversion
Source: "build-win-fixed\import_set_v4.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "build-win-fixed\import_set_v4.vbs"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
Name: "{app}"; Permissions: users-modify
Name: "{app}\Data"; Permissions: users-modify
Name: "{app}\Data\Fundamental"; Permissions: users-modify
Name: "{app}\Data\Fundamental\JSON"; Permissions: users-modify

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Run {#MyAppName}"; Flags: postinstall nowait skipifsilent
