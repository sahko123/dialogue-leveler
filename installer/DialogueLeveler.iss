; Dialogue Leveler — Inno Setup 6 installer script
; SPDX-License-Identifier: GPL-3.0-only
; Copyright (c) 2025 sahko123
;
; Build from repo root:
;   "C:\Program Files (x86)\Inno Setup 6\iscc.exe" installer\DialogueLeveler.iss

#define AppName      "Dialogue Leveler"
#define AppPublisher "sahko123"
; AppVersion can be overridden from the command line: iscc /DAppVersion=1.2.0 ...
; Falls back to 1.0.0 when building locally without the flag.
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define AppURL      "https://github.com/sahko123/dialogue-leveler"
#define VST3Src     "..\build\DialogueLeveler_artefacts\Release\VST3\DialogueLeveler.vst3"

[Setup]
; Unique ID — do not change once published (used for upgrades/uninstall tracking)
AppId={{D4A7B2E1-93CF-4F08-B15A-72E8C0314D6F}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}

; 64-bit only — VST3 on Windows is always 64-bit
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; VST3 goes to C:\Program Files\Common Files\VST3\ — fixed, no user choice
DefaultDirName={commonpf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
CreateUninstallRegKey=yes

LicenseFile=..\LICENSE
OutputDir=..\installer_output
OutputBaseFilename=DialogueLeveler-{#AppVersion}-Windows-Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin

VersionInfoVersion={#AppVersion}
VersionInfoDescription={#AppName} VST3 Plugin Installer
VersionInfoCompany={#AppPublisher}
VersionInfoCopyright=Copyright (C) 2025 {#AppPublisher}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; VST3 bundle — main plugin binary
Source: "{#VST3Src}\Contents\x86_64-win\DialogueLeveler.vst3"; \
    DestDir: "{commonpf64}\VST3\DialogueLeveler.vst3\Contents\x86_64-win"; \
    Flags: ignoreversion

; VST3 bundle — module metadata (required by VST3 spec)
Source: "{#VST3Src}\Contents\Resources\moduleinfo.json"; \
    DestDir: "{commonpf64}\VST3\DialogueLeveler.vst3\Contents\Resources"; \
    Flags: ignoreversion

[UninstallDelete]
; Remove the entire bundle folder on uninstall
Type: filesandordirs; Name: "{commonpf64}\VST3\DialogueLeveler.vst3"

[Messages]
WelcomeLabel1=Welcome to the {#AppName} installer
WelcomeLabel2=This will install {#AppName} {#AppVersion}, a free automatic dialogue leveler VST3 plugin for podcasts, video, and broadcast.%n%nThe plugin will be installed to:%n%n    C:\Program Files\Common Files\VST3\%n%nAfter installation, open your DAW and scan for new VST3 plugins.
FinishedHeadingLabel=Installation complete
FinishedLabel={#AppName} {#AppVersion} has been installed.%n%nOpen your DAW and scan for new VST3 plugins to start using it.%n%nSource code and documentation: {#AppURL}
