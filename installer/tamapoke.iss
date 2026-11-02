; Instalador do TamaPoke (Inno Setup 6, com ISPP).
; Uso:  iscc /DAppVersion=0.1.0 /DSourceDir=<pasta de staging do tools/package.ps1> installer\tamapoke.iss
; A pasta de staging so contem sprites se foi gerada com -WithSprites (o CI nao gera;
; os sprites sao CC BY-NC, ver CREDITS.md).

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\stage"
#endif

[Setup]
AppId={{4807ED87-4B8F-477C-98C9-316C634DBC71}
AppName=TamaPoke
AppVersion={#AppVersion}
AppPublisher=robso
PrivilegesRequired=lowest
DefaultDirName={localappdata}\Programs\TamaPoke
DefaultGroupName=TamaPoke
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=TamaPokeSetup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\assets\tamapoke.ico
UninstallDisplayIcon={app}\tamapoke.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
VersionInfoVersion={#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
#if FileExists(CompilerPath + "Languages\BrazilianPortuguese.isl")
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
#endif

[Messages]
WelcomeLabel2=Isto instala o [name/ver] no seu computador (apenas para o seu usuario).%n%nThis installs [name/ver] for your user only.%n%nSprites de Pokemon, se presentes, sao CC BY-NC 4.0 (uso pessoal, NAO comercial); creditos em CREDITS.md.%nPokemon sprites, if present, are CC BY-NC 4.0 (personal, NON-commercial use); credits are in CREDITS.md.

[CustomMessages]
AutostartTask=Start TamaPoke with Windows
AutostartGroup=Startup:
OpenApp=Open TamaPoke
AskRemoveData=Also remove your saved pet and settings (AppData\Roaming\TamaPoke)?
#if FileExists(CompilerPath + "Languages\BrazilianPortuguese.isl")
brazilianportuguese.AutostartTask=Iniciar o TamaPoke com o Windows
brazilianportuguese.AutostartGroup=Inicializacao:
brazilianportuguese.OpenApp=Abrir o TamaPoke
brazilianportuguese.AskRemoveData=Remover tambem o bicho salvo e as configuracoes (AppData\Roaming\TamaPoke)?
#endif

[Tasks]
Name: "autostart"; Description: "{cm:AutostartTask}"; GroupDescription: "{cm:AutostartGroup}"
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\TamaPoke"; Filename: "{app}\tamapoke.exe"
Name: "{autodesktop}\TamaPoke"; Filename: "{app}\tamapoke.exe"; Tasks: desktopicon

[Registry]
; Mesmo valor que o app grava a cada inicializacao: "<exe>" --hidden
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "TamaPoke"; ValueData: """{app}\tamapoke.exe"" --hidden"; Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "{app}\tamapoke.exe"; Description: "{cm:OpenApp}"; Flags: postinstall skipifsilent nowait

[Code]
// Pede ao TamaPoke em execucao para salvar e sair (fechar a janela so esconde o bicho).
// --quit devolve 0 se nao ha instancia; espera ate 10 s caso contrario.
procedure QuitRunningApp();
var
  ExePath: String;
  ResultCode: Integer;
begin
  ExePath := ExpandConstant('{app}\tamapoke.exe');
  if FileExists(ExePath) then
    Exec(ExePath, '--quit', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  QuitRunningApp();
  Result := '';
end;

// Sem "Iniciar com o Windows", grava settings.json com autostart=false (sem sobrescrever),
// senao o app recriaria o valor de inicializacao no primeiro uso (padrao: autostart=true).
procedure CurStepChanged(CurStep: TSetupStep);
var
  DataDir, SettingsFile: String;
begin
  if CurStep = ssPostInstall then
  begin
    if not WizardIsTaskSelected('autostart') then
    begin
      DataDir := ExpandConstant('{userappdata}\TamaPoke');
      SettingsFile := DataDir + '\settings.json';
      if not FileExists(SettingsFile) then
      begin
        ForceDirectories(DataDir);
        SaveStringToFile(SettingsFile, '{"autostart": false}', False);
      end;
    end;
  end;
end;

function InitializeUninstall(): Boolean;
begin
  QuitRunningApp();
  Result := True;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    RegDeleteValue(HKEY_CURRENT_USER, 'Software\Microsoft\Windows\CurrentVersion\Run', 'TamaPoke');
  if CurUninstallStep = usPostUninstall then
  begin
    // O save e as configuracoes so saem com confirmacao; desinstalacao silenciosa as mantem.
    if not UninstallSilent() then
      if MsgBox(ExpandConstant('{cm:AskRemoveData}'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(ExpandConstant('{userappdata}\TamaPoke'), True, True, True);
  end;
end;
