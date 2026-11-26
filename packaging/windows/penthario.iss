; Package the already-validated portable staging directory without changing user data.
#ifndef AppVersion
  #error AppVersion must be supplied by the packaging script
#endif
#ifndef SourceDirectory
  #error SourceDirectory must be supplied by the packaging script
#endif
#ifndef OutputDirectory
  #error OutputDirectory must be supplied by the packaging script
#endif
#ifndef InstallerAppId
  ; Stable upgrade identity from the original product name.
  #define UpgradeLegacyProduct 1
  #define InstallerAppId "io.github.hdijkema.AltPgAdmin"
#endif
#ifndef InstallerName
  #define InstallerName "Penthario"
#endif

[Setup]
AppId={#InstallerAppId}
AppName={#InstallerName}
AppVersion={#AppVersion}
AppPublisher=Penthario contributors
AppPublisherURL=https://hdijkema.github.io/penthario/
DefaultDirName={autopf}\penthario
DefaultGroupName={#InstallerName}
DisableProgramGroupPage=yes
AllowNoIcons=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDirectory}
OutputBaseFilename=penthario-{#AppVersion}-windows-x64-setup
SetupIconFile=..\..\assets\icons\penthario.ico
UninstallDisplayIcon={app}\penthario.exe
LicenseFile=..\..\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no
RestartApplications=no
UninstallDisplayName={#InstallerName} {#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDirectory}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#InstallerName}"; Filename: "{app}\penthario.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\{#InstallerName}"; Filename: "{app}\penthario.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[InstallDelete]
; Remove superseded program files/shortcuts only; user SQL and settings are retained.
Type: files; Name: "{app}\pgadmin-jr.exe"
#ifdef UpgradeLegacyProduct
Type: files; Name: "{autoprograms}\alt-pg-admin.lnk"
Type: files; Name: "{autodesktop}\alt-pg-admin.lnk"
Type: files; Name: "{autoprograms}\pgadmin-jr.lnk"
Type: files; Name: "{autodesktop}\pgadmin-jr.lnk"
#endif

[Run]
Filename: "{app}\penthario.exe"; Description: "{cm:LaunchProgram,{#InstallerName}}"; Flags: nowait postinstall skipifsilent
