<#
.SYNOPSIS
  Gera o zip portatil do TamaPoke a partir de um build ja compilado.
.DESCRIPTION
  Roda "cmake --install" por componente numa pasta de staging limpa e compacta
  dist/TamaPoke-<versao>-win64[-sprites].zip (uma pasta raiz TamaPoke-<versao>-win64/).
  Por padrao NAO inclui os sprites (CC BY-NC, ver CREDITS.md); use -WithSprites para
  o pacote pessoal. A versao vem de project(VERSION) em CMakeLists.txt, via
  <BuildDir>/gen/version.txt (escrito pelo CMake na configuracao).
.EXAMPLE
  pwsh tools/package.ps1 -Config Release
  powershell -File tools/package.ps1 -WithSprites
#>
[CmdletBinding()]
param(
  [string]$Config = 'Release',
  [string]$BuildDir = 'build',
  [string]$OutDir = 'dist',
  [switch]$WithSprites
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Resolve-Full([string]$p) {
  if ([System.IO.Path]::IsPathRooted($p)) { return [System.IO.Path]::GetFullPath($p) }
  return [System.IO.Path]::GetFullPath((Join-Path $root $p))
}
$build = Resolve-Full $BuildDir
$out = Resolve-Full $OutDir

$exe = Join-Path $build "$Config\tamapoke.exe"
$verFile = Join-Path $build 'gen\version.txt'
if (-not (Test-Path $exe) -or -not (Test-Path $verFile)) {
  throw "Build $Config nao encontrado em '$build' (faltam tamapoke.exe ou gen\version.txt). Rode: cmake -B build -G `"Visual Studio 17 2022`" -A x64 ; cmake --build build --config $Config"
}
$version = (Get-Content $verFile -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "Versao invalida em ${verFile}: '$version'" }

$name = "TamaPoke-$version-win64"
$zipName = if ($WithSprites) { "$name-sprites.zip" } else { "$name.zip" }
New-Item -ItemType Directory -Force $out | Out-Null
$zip = Join-Path $out $zipName

# Staging limpo: <out>/stage-<zipbase>/<name>/...
$stageRoot = Join-Path $out ('stage-' + [System.IO.Path]::GetFileNameWithoutExtension($zipName))
if (Test-Path $stageRoot) { Remove-Item -Recurse -Force $stageRoot }
$stage = Join-Path $stageRoot $name
New-Item -ItemType Directory -Force $stage | Out-Null

$components = @('app')
if ($WithSprites) { $components += 'sprites' }
foreach ($c in $components) {
  & cmake --install $build --config $Config --prefix $stage --component $c | Out-Null
  if ($LASTEXITCODE -ne 0) { throw "cmake --install (componente $c) falhou ($LASTEXITCODE)" }
}

# cmake -E tar --format=zip grava entradas com '/' e a pasta raiz dada.
if (Test-Path $zip) { Remove-Item -Force $zip }
Push-Location $stageRoot
try {
  & cmake -E tar cf $zip --format=zip -- $name
  if ($LASTEXITCODE -ne 0) { throw "cmake -E tar falhou ($LASTEXITCODE)" }
} finally { Pop-Location }

$info = Get-Item $zip
Write-Host "Versao:  $version ($Config, $(if ($WithSprites) { 'com sprites - so uso pessoal' } else { 'sem sprites' }))"
Write-Host "Staging: $stage"
Write-Host ("Zip:     {0} ({1:N0} bytes)" -f $info.FullName, $info.Length)
