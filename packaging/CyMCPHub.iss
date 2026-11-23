; SPDX-FileCopyrightText: 2026 Cyberalien
; SPDX-License-Identifier: GPL-3.0-only

#define HubAppId "{{B16C9BCD-5D03-4198-9C2E-E24D29E9A5DE}"
#define HubRunKey "Software\Microsoft\Windows\CurrentVersion\Run"
#define HubRunValue "CyMCPHub"

[Setup]
AppId={#HubAppId}
AppName=CyMCPHub
AppVersion={#AppVersion}
AppPublisher=Cyberalien
AppPublisherURL=https://github.com/MrMybal/CyMCPHub
DefaultDirName={localappdata}\Programs\CyMCPHub
DefaultGroupName=CyMCPHub
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputPath}
OutputBaseFilename=CyMCPHub-Setup-{#AppVersion}-windows-x64
SetupIconFile=..\assets\branding\cymcphub.ico
LicenseFile=..\LICENSE
UninstallDisplayIcon={app}\CyMCPHub.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
DisableProgramGroupPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[CustomMessages]
english.StartWithWindows=Start CyMCPHub at Windows sign-in
french.StartWithWindows=Démarrer CyMCPHub à l’ouverture de session Windows

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked
Name: "startwithwindows"; Description: "{cm:StartWithWindows}"; Check: OfferWindowsStartup

[Files]
Source: "{#StagePath}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
Root: HKCU; Subkey: "{#HubRunKey}"; ValueType: string; ValueName: "{#HubRunValue}"; ValueData: """{app}\CyMCPHub.exe"" --tray --workspace ""{%USERPROFILE}"""; Tasks: startwithwindows; Check: OfferWindowsStartup; Flags: createvalueifdoesntexist

[Icons]
Name: "{autoprograms}\CyMCPHub"; Filename: "{app}\CyMCPHub.exe"
Name: "{autodesktop}\CyMCPHub"; Filename: "{app}\CyMCPHub.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\CyMCPHub.exe"; Description: "{cm:LaunchProgram,CyMCPHub}"; Flags: nowait postinstall skipifsilent

[Code]
var
  NewWindowsStartup: Boolean;

function InitializeSetup: Boolean;
begin
  NewWindowsStartup :=
    not RegKeyExists(HKCU, ExpandConstant('Software\Microsoft\Windows\CurrentVersion\Uninstall\{#HubAppId}_is1')) and
    not RegValueExists(HKCU, '{#HubRunKey}', '{#HubRunValue}');
  Result := True;
end;

function OfferWindowsStartup: Boolean;
begin
  Result := NewWindowsStartup;
end;

procedure CurUninstallStepChanged(Step: TUninstallStep);
var
  Command, ProgramPath: String;
begin
  if Step = usPostUninstall then
  begin
    ProgramPath := Lowercase(ExpandConstant('"{app}\CyMCPHub.exe"'));
    if RegQueryStringValue(HKCU, '{#HubRunKey}', '{#HubRunValue}', Command) and
       (Pos(ProgramPath, Lowercase(Command)) = 1) then
      RegDeleteValue(HKCU, '{#HubRunKey}', '{#HubRunValue}');
  end;
end;
