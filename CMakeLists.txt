cmake_minimum_required(VERSION 3.22)
project(NewProject VERSION 0.1.0)
if (NOT DEFINED JUCE_DIR AND DEFINED ENV{JUCE_DIR} AND EXISTS "$ENV{JUCE_DIR}/CMakeLists.txt")
    set(JUCE_DIR "$ENV{JUCE_DIR}")
endif ()
set(JUCE_DIR "${JUCE_DIR}" CACHE PATH "Path to the JUCE folder")
if (NOT JUCE_DIR)
    set(JUCE_DIR "C:/JUCE" CACHE PATH "Path to the JUCE folder" FORCE)
endif ()
if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE_DIR must point to your JUCE folder. Current value: '${JUCE_DIR}'. "
        "Set the JUCE_DIR cache variable or JUCE_DIR environment variable.")
endif ()
add_subdirectory(${JUCE_DIR} JUCE_build)
option(NEWPROJECT_DEPLOY_BUILD "Enable post-build copy into the system VST3 folder" OFF)
if (NEWPROJECT_DEPLOY_BUILD)
    set(NEWPROJECT_COPY_PLUGIN_AFTER_BUILD ON CACHE BOOL "Copy built plugin into the system VST3 folder after building" FORCE)
else ()
    set(NEWPROJECT_COPY_PLUGIN_AFTER_BUILD OFF CACHE BOOL "Copy built plugin into the system VST3 folder after building" FORCE)
endif ()
set(NEWPROJECT_PLUGIN_COPY_AFTER_BUILD_ARG)
if (NEWPROJECT_COPY_PLUGIN_AFTER_BUILD)
    list(APPEND NEWPROJECT_PLUGIN_COPY_AFTER_BUILD_ARG COPY_PLUGIN_AFTER_BUILD TRUE)
endif ()
juce_add_plugin(NewProject
    COMPANY_NAME            "Alletis"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    AU_MAIN_TYPE            kAudioUnitType_MusicEffect
    ${NEWPROJECT_PLUGIN_COPY_AFTER_BUILD_ARG}
    PLUGIN_MANUFACTURER_CODE Alle
    PLUGIN_CODE              Fluo
    FORMATS                  AU VST3 Standalone
    PRODUCT_NAME             "Alletis Fluorescence")
target_sources(NewProject PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)
juce_add_binary_data(NewProjectData
    SOURCES
        Resources/logo.png
        Resources/brand.png
        Resources/brand2.png
        Resources/eye.png)
target_compile_definitions(NewProject PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_compile_options(NewProject PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-math-errno -fno-trapping-math>
    $<$<CXX_COMPILER_ID:MSVC>:/fp:fast>)
target_link_libraries(NewProject
    PRIVATE
        NewProjectData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
if (TARGET NewProject_VST3)
    add_custom_command(TARGET NewProject_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove -f
            "$<TARGET_FILE_DIR:NewProject_VST3>/../../../AlletisAuthenticator.exe"
            "$<TARGET_FILE_DIR:NewProject_VST3>/../../AlletisAuthenticator.exe"
            "$<TARGET_FILE_DIR:NewProject_VST3>/../AlletisAuthenticator.exe"
            "$<TARGET_FILE_DIR:NewProject_VST3>/AlletisAuthenticator.exe"
        VERBATIM)
endif ()