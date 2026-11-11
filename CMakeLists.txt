cmake_minimum_required(VERSION 3.22)
project(KingLionBox VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
endif()

# JUCE 8.0.4: use -DJUCE_DIR=<pasta> ou deixe o CMake baixar.
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(KLBData SOURCES
    Resources/skin.jpg
    Resources/fonts/BarlowCondensed-SemiBold.ttf
    Resources/fonts/BarlowCondensed-Bold.ttf)

# Código compartilhado entre o plugin e os testes de integração.
add_library(KLBShared INTERFACE)
target_sources(KLBShared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Engine/KingLionEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Parameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Widgets.cpp)
target_include_directories(KLBShared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_definitions(KLBShared INTERFACE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 KLB_VERSION="${PROJECT_VERSION}")
target_link_libraries(KLBShared INTERFACE KLBData
    juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra)

set(KLB_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND KLB_FORMATS AU)
endif()

juce_add_plugin(KingLionBox
    COMPANY_NAME "WYATT Sounds"
    COMPANY_WEBSITE ""
    BUNDLE_ID com.wyattsounds.kinglionbox
    PLUGIN_MANUFACTURER_CODE Wytt
    PLUGIN_CODE Klbx
    PRODUCT_NAME "KingLion Box"
    FORMATS ${KLB_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    DESCRIPTION "Tube voice channel: opto compressor, tube preamp, peak-dip EQ, de-esser/limiter")

target_link_libraries(KingLionBox PRIVATE KLBShared
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------- testes ----------
option(KLB_TESTS "Compilar os testes" ON)
if(KLB_TESTS)
    add_executable(KingLionEngineTest Tests/EngineTest.cpp Source/Engine/KingLionEngine.cpp)
    target_include_directories(KingLionEngineTest PRIVATE Source)

    juce_add_console_app(KingLionIntegration PRODUCT_NAME "KingLionIntegration")
    target_sources(KingLionIntegration PRIVATE Tests/IntegrationTest.cpp)
    target_compile_definitions(KingLionIntegration PRIVATE JUCE_STANDALONE_APPLICATION=0)
    target_link_libraries(KingLionIntegration PRIVATE KLBShared juce::juce_recommended_config_flags)
endif()
