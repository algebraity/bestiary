@echo off
setlocal

set "SOURCE_DIR=%~dp0"
set "DEFAULT_DIR=%LOCALAPPDATA%\Bestiary"

echo Bestiary Windows installer
echo.
echo This installer copies Bestiary and its required DLLs into one folder.
echo The DLLs are kept next to the exe files so Windows can find them safely.
echo.
set "INSTALL_DIR=%DEFAULT_DIR%"
set /p "CUSTOM_DIR=Install location [%DEFAULT_DIR%]: "
if not "%CUSTOM_DIR%"=="" set "INSTALL_DIR=%CUSTOM_DIR%"

if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
if errorlevel 1 (
    echo Failed to create "%INSTALL_DIR%".
    exit /b 1
)

echo.
echo Copying files...
xcopy "%SOURCE_DIR%*" "%INSTALL_DIR%\" /E /I /Y /Q >nul
if errorlevel 1 (
    echo Failed to copy Bestiary files.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$start=[Environment]::GetFolderPath('Programs');" ^
    "$dir=Join-Path $start 'Bestiary';" ^
    "New-Item -ItemType Directory -Force -Path $dir | Out-Null;" ^
    "$ws=New-Object -ComObject WScript.Shell;" ^
    "$s=$ws.CreateShortcut((Join-Path $dir 'Bestiary GUI.lnk'));" ^
    "$s.TargetPath=Join-Path '%INSTALL_DIR%' 'bestiary-gui.exe';" ^
    "$s.WorkingDirectory='%INSTALL_DIR%';" ^
    "$s.Save();" ^
    "$s=$ws.CreateShortcut((Join-Path $dir 'Bestiary CLI.lnk'));" ^
    "$s.TargetPath=Join-Path '%INSTALL_DIR%' 'bestiary.exe';" ^
    "$s.WorkingDirectory='%INSTALL_DIR%';" ^
    "$s.Save();" >nul 2>nul

echo.
echo Installed Bestiary to:
echo   %INSTALL_DIR%
echo.
echo Start Menu shortcuts were created when supported.
echo Keep the DLL files in this folder with the exe files.
echo.
pause
