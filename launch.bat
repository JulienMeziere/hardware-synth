@echo off
:: Get the directory where this script is located
set BATCH_DIR=%~dp0

:: Set the default build configuration to Debug
set BUILD_CONFIG=Debug

:: Check for optional --release argument
if "%1"=="--release" (
    set BUILD_CONFIG=Release
)

:: Run the VST in the editor host
%BATCH_DIR%libs\vst3sdk\build\bin\Release\editorhost.exe %BATCH_DIR%build\VST3\%BUILD_CONFIG%\Hardware_Synth.vst3\Contents\x86_64-win\Hardware_Synth.vst3
