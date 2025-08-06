@echo off
setlocal enabledelayedexpansion
set BATCH_DIR=%~dp0

:: Set the default build configuration to Debug
set BUILD_CONFIG=Debug

:: Check if we've already set BUILD_CONFIG before asking for admin
if not exist "%temp%\build_config.txt" (
    :: Check for optional --release argument
    if "%1"=="--release" (
        set BUILD_CONFIG=Release
    )
    
    echo !BUILD_CONFIG!
    :: Save the BUILD_CONFIG to a temporary file
    echo !BUILD_CONFIG!> "%temp%\build_config.txt"
)

:: Request admin rights
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"

:: Check error level returned from cacls
if '%errorlevel%' NEQ '0' (
    echo Requesting administrative privileges...
    goto UACPrompt
) else (
    goto gotAdmin
)

:UACPrompt
    echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
    echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%temp%\getadmin.vbs"

    "%temp%\getadmin.vbs"
    exit /B

:gotAdmin
    :: Read the BUILD_CONFIG back from the temporary file
    set /p BUILD_CONFIG=<%temp%\build_config.txt
    del "%temp%\getadmin.vbs"
    del "%temp%\build_config.txt"

    echo ========================================
    echo Hardware Synth - Copy to DAW
    echo ========================================
    echo.
    echo Checking for processes using the VST file...
    
    :: Try to close any processes that might be using the VST file
    taskkill /f /im "FL64.exe" >nul 2>&1
    taskkill /f /im "Ableton Live 11.exe" >nul 2>&1
    taskkill /f /im "Ableton Live 12.exe" >nul 2>&1
    taskkill /f /im "Reaper.exe" >nul 2>&1
    taskkill /f /im "Cubase.exe" >nul 2>&1
    taskkill /f /im "Studio One.exe" >nul 2>&1
    
    echo Waiting for file handles to be released...
    timeout /t 3 /nobreak >nul
    
    echo Copying VST plugin to DAW folder...
    xcopy "%BATCH_DIR%build\VST3\%BUILD_CONFIG%\Hardware_Synth.vst3" "C:\Program Files\Common Files\VST3\Hardware_Synth\" /E /H /Y

    if %errorlevel% equ 0 (
        echo.
        echo ✓ Success! VST plugin copied to DAW folder.
        echo.
        echo You can now:
        echo - Open your DAW
        echo - Load the "Hardware Synth" VST plugin
        echo - Connect your Arduino Uno to see CV outputs
    ) else (
        echo.
        echo ✗ Fail. Error code: %errorlevel%
        echo.
        echo Troubleshooting:
        echo - Close your DAW completely
        echo - Wait a few seconds
        echo - Try running this script again
        echo - Or restart your computer if the issue persists
    )

pause
