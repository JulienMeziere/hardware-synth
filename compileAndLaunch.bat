@echo off
setlocal

:: Check for optional --release argument and pass it to the scripts
set BUILD_OPTION=
if "%1"=="--release" (
    set BUILD_OPTION=--release
)

:: Call the compile.bat script
call compile.bat %BUILD_OPTION%
if %ERRORLEVEL% NEQ 0 (
    echo Compile failed, not launching.
    exit /b %ERRORLEVEL%
)

:: Call the launchVSTInEditor.bat script
call launch.bat %BUILD_OPTION%

endlocal
