cmake_minimum_required(VERSION 3.22)

# Mac : binaire universel (Apple Silicon + Intel)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(SwellTape VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE : on réutilise le checkout de Swell Reverse s'il est là, sinon on le télécharge (CI).
set(SWELL_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Swell-Reverse/JUCE" CACHE PATH "JUCE checkout")
include(FetchContent)
if(EXISTS "${SWELL_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${SWELL_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

# Monocypher : vérification Ed25519 des fichiers de licence.
FetchContent_Declare(monocypher GIT_REPOSITORY https://github.com/LoupVaillant/Monocypher.git GIT_TAG 4.0.3 GIT_SHALLOW TRUE SOURCE_SUBDIR none)
FetchContent_MakeAvailable(monocypher)
add_library(monocypher STATIC "${monocypher_SOURCE_DIR}/src/monocypher.c" "${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c")
target_include_directories(monocypher PUBLIC "${monocypher_SOURCE_DIR}/src" "${monocypher_SOURCE_DIR}/src/optional")
set_target_properties(monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(SWELL_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SWELL_FORMATS AU)
endif()

juce_add_plugin(SwellTape
    COMPANY_NAME "Swell-audio"
    BUNDLE_ID com.sevyaudio.swelltape
    PLUGIN_MANUFACTURER_CODE Sevy
    PLUGIN_CODE Stap
    FORMATS ${SWELL_FORMATS}
    PRODUCT_NAME "Swell Tape"
    DESCRIPTION "Vintage tape FX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Distortion)

set(SWELL_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/TapeEngine.cpp
    Source/Licence/SwellLicence.cpp)

juce_add_binary_data(SwellTapeAssets SOURCES
    Assets/swell_mark.png
    Assets/geist_regular.ttf
    Assets/geist_semibold.ttf
    Assets/reel.png
    Assets/transport.jpg)

set(SWELL_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    SWELL_VERSION="${PROJECT_VERSION}")

target_sources(SwellTape PRIVATE ${SWELL_SOURCES})
target_compile_definitions(SwellTape PUBLIC ${SWELL_DEFS})
target_link_libraries(SwellTape
    PRIVATE monocypher juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp SwellTapeAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Banc de test hors-ligne (rendus WAV, contrôles audio, captures PNG), optionnel
option(SWELL_BUILD_TESTS "Build offline render test" OFF)
if(SWELL_BUILD_TESTS)
    juce_add_console_app(SwellTapeTest PRODUCT_NAME "SwellTapeTest")
    target_sources(SwellTapeTest PRIVATE Tests/TapeTest.cpp ${SWELL_SOURCES})
    target_compile_definitions(SwellTapeTest PRIVATE ${SWELL_DEFS} JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(SwellTapeTest
        PRIVATE monocypher juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp SwellTapeAssets
            juce::juce_recommended_config_flags)
endif()

# Vérification du composant AU installé, chargé comme dans un hôte
option(SWELL_BUILD_AUHOST "Build AU host check" OFF)
if(SWELL_BUILD_AUHOST AND APPLE)
    juce_add_console_app(AuHostCheck PRODUCT_NAME "AuHostCheck")
    target_sources(AuHostCheck PRIVATE Tests/AuHostCheck.cpp)
    target_compile_definitions(AuHostCheck PRIVATE JUCE_PLUGINHOST_AU=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(AuHostCheck PRIVATE juce::juce_audio_processors juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
