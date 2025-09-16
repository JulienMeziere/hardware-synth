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
    set DEST_DIR=C:\Program Files\Common Files\VST3\Hardware_Synth
    :check_lock
    echo Checking whether the installed VST is in use...
    powershell -NoProfile -Command "$p='%DEST_DIR%'; if (Test-Path $p) { $locked=$false; foreach ($f in Get-ChildItem -Path $p -Recurse -File) { try { $s=[System.IO.File]::Open($f.FullName,[System.IO.FileMode]::Open,[System.IO.FileAccess]::ReadWrite,[System.IO.FileShare]::None); $s.Close() } catch { $locked=$true; break } }; if ($locked) { exit 1 } else { exit 0 } } else { exit 0 }"
    if %errorlevel% neq 0 (
        echo.
        echo The VST plugin is currently in use.
        echo Close plugin instances in your DAW.
        echo Press any key to retry, or Ctrl+C to abort...
        pause >nul
        goto check_lock
    )

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

:end
pause
