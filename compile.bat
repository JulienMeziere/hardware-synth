@echo off
setlocal

:: Set the default build configuration to Debug
set BUILD_CONFIG=Debug

:: Check for optional --release argument
if "%1"=="--release" (
    set BUILD_CONFIG=Release
    echo ========================================
    echo Hardware Synth - RELEASE BUILD
    echo ========================================
    echo.
    echo Building optimized release version...
    echo - No logging overhead
    echo - Maximum performance
    echo - Optimized binary size
    echo.
) else (
    echo ========================================
    echo Hardware Synth - DEBUG BUILD
    echo ========================================
    echo.
    echo Building debug version...
    echo - Full logging enabled
    echo - Debug information included
    echo.
)

:: Change to the directory where the batch file is located
set SCRIPT_DIR=%~dp0
cd %SCRIPT_DIR%

:: Source the Developer Command Prompt for Visual Studio to set environment variables
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"

:: Run CMake commands
cmake -S . -B build
cmake --build build --config %BUILD_CONFIG% --target Hardware_Synth

if %errorlevel% equ 0 (
    echo.
    if "%BUILD_CONFIG%"=="Release" (
        echo ✓ RELEASE BUILD SUCCESSFUL!
        echo.
        echo Release optimizations:
        echo - Logging completely disabled
        echo - Zero logging overhead
        echo - Optimized for performance
        echo.
        echo Your VST plugin is ready for distribution!
    ) else (
        echo ✓ DEBUG BUILD SUCCESSFUL!
        echo.
        echo Debug features:
        echo - Full logging to serialLogs.txt
        echo - Debug symbols included
        echo - Ready for development testing
    )
) else (
    echo.
    echo ✗ Build failed!
    echo Check the errors above.
)

endlocal
