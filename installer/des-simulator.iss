; ============================================================================
; installer/des-simulator.iss  --  the Windows installer
; ============================================================================
; Built by tools/package.sh, which stages dist/DES-Simulator first and passes
; the version in. It can also be compiled by hand once that folder exists:
;
;     "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\des-simulator.iss
;
; PER-USER BY DEFAULT. PrivilegesRequired=lowest installs into the user's own
; Programs folder and writes file types under HKCU, so it works on a laptop
; whose owner is not an administrator -- which is most university and company
; laptops, and exactly the people this is for. Someone who is an administrator
; can still choose an all-users install in the dialog.

#define AppName "DES Simulator"
#define AppExe  "DES-Simulator.exe"

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\DES-Simulator"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

[Setup]
; The AppId must NEVER change: it is how a later installer recognises an earlier
; install and upgrades it rather than installing a second copy beside it.
AppId={{6F1C2B7A-4E3D-4C9B-9A5E-2D7B8C1F0A43}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Musaib
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#OutputDir}
OutputBaseFilename=DES-Simulator-{#AppVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
ChangesAssociations=yes
UninstallDisplayIcon={app}\{#AppExe}
DisableProgramGroupPage=yes

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "associate"; Description: "Open .des model files with {#AppName} when double-clicked"; GroupDescription: "File types:"

[Files]
Source: "{#SourceDir}\{#AppExe}";      DestDir: "{app}";          Flags: ignoreversion
Source: "{#SourceDir}\README.txt";     DestDir: "{app}";          Flags: ignoreversion isreadme
Source: "{#SourceDir}\examples\*.des"; DestDir: "{app}\examples"; Flags: ignoreversion

[Icons]
; Started from a shortcut, the working folder is the user's Documents rather
; than the install folder -- which a per-machine install cannot write to.
Name: "{group}\{#AppName}";   Filename: "{app}\{#AppExe}"; WorkingDir: "{userdocs}"
Name: "{group}\Example models"; Filename: "{app}\examples"
Name: "{group}\Read me";      Filename: "{app}\README.txt"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{userdocs}"; Tasks: desktopicon

[Registry]
; HKA is HKCU for a per-user install and HKLM for an all-users one, so the file
; type lands wherever the program did. The association is an opt-in task
; because .des is not reserved to this program, and taking over an extension
; somebody else's software uses without asking is not ours to do.
Root: HKA; Subkey: "Software\Classes\.des"; ValueType: string; ValueName: ""; ValueData: "DESSimulator.Model"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.des\OpenWithProgids"; ValueType: string; ValueName: "DESSimulator.Model"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\DESSimulator.Model"; ValueType: string; ValueName: ""; ValueData: "{#AppName} model"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\DESSimulator.Model\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: associate
Root: HKA; Subkey: "Software\Classes\DESSimulator.Model\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: associate

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; WorkingDir: "{userdocs}"; Flags: nowait postinstall skipifsilent
