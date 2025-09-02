cmake_minimum_required(VERSION 3.14.0)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")

set(vst3sdk_SOURCE_DIR "${CMAKE_SOURCE_DIR}/libs/vst3sdk")

if(NOT vst3sdk_SOURCE_DIR)
    message(FATAL_ERROR "Path to VST3 SDK is empty!")
endif()

project(Hardware_Synth

    # This is your plug-in version number. Change it here only.
    # Version number symbols usable in C++ can be found in
    # source/version.h and ${PROJECT_BINARY_DIR}/projectversion.h.
    VERSION 1.0.0.0
    DESCRIPTION "Hardware Synth VST 3 Plug-in"
)

set(SMTG_VSTGUI_ROOT "${vst3sdk_SOURCE_DIR}")

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)

smtg_enable_vst3_sdk()

smtg_add_vst3plugin(Hardware_Synth

    # Global
    source/version.h
    source/cids.h
    source/entry.cpp
    source/Logger.h
    source/Hardware/MIDIDevices.h
    source/Hardware/MIDIDevices.cpp
    source/Hardware/HardwareSynthesizer.h
    source/Hardware/HardwareSynthesizer.cpp

    # Processor
    source/Processor/Processor.h
    source/Processor/Processor.cpp

    # UI
    source/UI/Controller.h
    source/UI/Controller.cpp
    source/UI/constants/colors.h
    source/UI/constants/colors.cpp
)

# Set build-specific compiler definitions
target_compile_definitions(Hardware_Synth
    PRIVATE
    $<$<CONFIG:Debug>:HARDWARE_SYNTH_DEBUG=1>
    $<$<CONFIG:Release>:HARDWARE_SYNTH_RELEASE=1>
)

# - VSTGUI Wanted ----
if(SMTG_ENABLE_VSTGUI_SUPPORT)
    target_sources(Hardware_Synth
        PRIVATE
        resource/editor.uidesc
    )
    target_link_libraries(Hardware_Synth
        PRIVATE
        vstgui_support
    )
    smtg_target_add_plugin_resources(Hardware_Synth
        RESOURCES
        "resource/editor.uidesc"
    )
endif(SMTG_ENABLE_VSTGUI_SUPPORT)

# -------------------
smtg_target_add_plugin_snapshots(Hardware_Synth
    RESOURCES
    resource/0A4237376BBC558E8736B74EE4D15F73_snapshot.png
    resource/0A4237376BBC558E8736B74EE4D15F73_snapshot_2.0x.png
)

target_link_libraries(Hardware_Synth
    PRIVATE
    sdk
)

# Link Windows multimedia library for MIDI device enumeration
if(WIN32)
    target_link_libraries(Hardware_Synth PRIVATE winmm)
endif()

smtg_target_configure_version_file(Hardware_Synth)

if(SMTG_MAC)
    smtg_target_set_bundle(Hardware_Synth
        BUNDLE_IDENTIFIER com.newkon.hardwaresynth
        COMPANY_NAME "Newkon"
    )
    smtg_target_set_debug_executable(Hardware_Synth
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
elseif(SMTG_WIN)
    target_sources(Hardware_Synth PRIVATE
        resource/win32resource.rc
    )

    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Hardware_Synth)

        smtg_target_set_debug_executable(Hardware_Synth
            "$(ProgramW6432)/Steinberg/VST3PluginTestHost/VST3PluginTestHost.exe"
            "--pluginfolder \"$(OutDir)/\""
        )
    endif()
endif(SMTG_MAC)
