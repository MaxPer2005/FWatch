# Установка и запуск FWatch-клиента на Windows одной командой (PowerShell):
#   irm https://raw.githubusercontent.com/MaxPer2005/FWatch/main/install.ps1 | iex
$ErrorActionPreference = "Stop"

$Server = "91.228.153.31"
$Port   = "9000"
$Repo   = "MaxPer2005/FWatch"
$Dest   = Join-Path $env:LOCALAPPDATA "FWatch"
$Exe    = Join-Path $Dest "sync.exe"

Write-Host ">> качаю sync.exe..."
New-Item -ItemType Directory -Force -Path $Dest | Out-Null
Invoke-WebRequest -Uri "https://github.com/$Repo/releases/latest/download/sync.exe" -OutFile $Exe
# снять метку "скачано из интернета" (SmartScreen)
Unblock-File -Path $Exe -ErrorAction SilentlyContinue

Write-Host ">> установлено: $Exe"
Write-Host ">> подключаюсь к ${Server}:${Port} ..."
Write-Host ""
Write-Host "   Запустить вручную потом:  $Exe client $Server $Port"
Write-Host ""

& $Exe client $Server $Port
