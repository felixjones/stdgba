@echo off
setlocal enabledelayedexpansion

set MGBA=C:\Program Files\mGBA\mGBA.exe
set SCRIPT=%~dp0screenshot.lua
set IMG_DIR=%~dp0..\src\img
set BUILD_DIR=%~dp0..\..\build\book\demos

if not exist "%MGBA%" (
    echo mGBA not found at %MGBA%
    exit /b 1
)

if not exist "%BUILD_DIR%" (
    echo Build directory not found: %BUILD_DIR%
    echo Build the demos first.
    exit /b 1
)

mkdir "%IMG_DIR%" 2>nul

for %%f in ("%BUILD_DIR%\demo_*.elf") do (
    set "name=%%~nf"
    set "screenshot_name=!name:demo_=!"

    rem Delete any previous mGBA screenshots for this ROM
    del /q "%%~dpf!name!-*.png" 2>nul

    echo !screenshot_name!...
    start "" "%MGBA%" -C updateAutoCheck=0 -C skipBios=1 --script "%SCRIPT%" "%%f"
    ping -n 4 127.0.0.1 >nul
    taskkill /F /IM mGBA.exe >nul 2>&1

    rem mGBA saves <romname>-0.png next to the ROM
    if exist "%%~dpf!name!-0.png" (
        move /y "%%~dpf!name!-0.png" "%IMG_DIR%\!screenshot_name!.png" >nul
        echo   ok
    ) else (
        echo   FAILED - no screenshot found
    )
)

echo Done. Screenshots in %IMG_DIR%
