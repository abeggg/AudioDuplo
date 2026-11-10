@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Build.ps1" -Package
if errorlevel 1 exit /b 1
echo Build and tests completed. See build\AudioDuplo-1.1.4-windows-x64.zip
