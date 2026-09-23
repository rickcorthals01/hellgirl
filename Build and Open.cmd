@echo off
setlocal
set "HellgirlEngine=C:\Program Files\Epic Games\UE_5.8"
set "HellgirlProject=%~dp0AshenArena.uproject"
echo Close Unreal Editor before running this file.
echo Building Hellgirl...
if not exist "%~dp0Logs" mkdir "%~dp0Logs"
call "%HellgirlEngine%\Engine\Build\BatchFiles\Build.bat" AshenArenaEditor Win64 Development "-Project=%HellgirlProject%" -WaitMutex "-Log=%~dp0Logs\BuildLog.txt"
if errorlevel 1 (
    echo.
    echo Build failed. The details are saved in Logs\BuildLog.txt.
    pause
    exit /b 1
)
echo Build succeeded. Opening Unreal Editor...
start "" "%HellgirlEngine%\Engine\Binaries\Win64\UnrealEditor.exe" "%HellgirlProject%"
endlocal
