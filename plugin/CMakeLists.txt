# ---------------------------------------------------------------------------
# Vendored upstream DSP, compiled unmodified (see plugin/vendor/VENDORED.json)
# ---------------------------------------------------------------------------
set(V ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
set(DUSK ${V}/dusk-audio/plugins)

add_library(plugsalot_upstream_dsp STATIC
    ${DUSK}/multi-comp/core/MultiCompDSP.cpp
    ${DUSK}/TapeMachine/core/TapeMachineDSP.cpp
    ${DUSK}/shared-daf/dsp/FourKEQDSP.cpp
    ${V}/x42-dpl/src/peaklim.cc
    ${V}/airwindows/plugins/LinuxVST/src/DeBess/DeBess.cpp
    ${V}/airwindows/plugins/LinuxVST/src/DeBess/DeBessProc.cpp
)
target_include_directories(plugsalot_upstream_dsp PUBLIC
    ${DUSK}/multi-comp/core
    ${DUSK}/TapeMachine/core
    ${DUSK}/shared-daf/dsp
    ${V}/x42-dpl/src
    ${V}/airwindows/plugins/LinuxVST/src/DeBess
    ${CMAKE_CURRENT_SOURCE_DIR}/shims/vst2
)
# Each Airwindows effect defines the same entry point; give each a unique name.
set_source_files_properties(
    ${V}/airwindows/plugins/LinuxVST/src/DeBess/DeBess.cpp
    PROPERTIES COMPILE_DEFINITIONS "createEffectInstance=createEffectInstance_DeBess")
if(MSVC)
    target_compile_options(plugsalot_upstream_dsp PRIVATE /W0 /D_USE_MATH_DEFINES /bigobj)
else()
    target_compile_options(plugsalot_upstream_dsp PRIVATE -w)
endif()

# Dragonfly Plate + freeverb3: older code with C++98 exception specifications,
# so it builds as its own C++14 library.
set(DF ${V}/dragonfly-reverb)
set(FV ${DF}/common/freeverb)
add_library(plugsalot_dragonfly STATIC
    ${DF}/plugins/dragonfly-plate-reverb/DSP.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/modules/DragonflyBridge.cpp
    ${FV}/allpass.cpp ${FV}/biquad.cpp ${FV}/comb.cpp ${FV}/delay.cpp ${FV}/delayline.cpp
    ${FV}/earlyref.cpp ${FV}/efilter.cpp ${FV}/nrev.cpp ${FV}/nrevb.cpp ${FV}/progenitor.cpp
    ${FV}/progenitor2.cpp ${FV}/revbase.cpp ${FV}/slot.cpp ${FV}/strev.cpp ${FV}/utils.cpp
    ${FV}/zrev.cpp ${FV}/zrev2.cpp
)
set_target_properties(plugsalot_dragonfly PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(plugsalot_dragonfly PUBLIC LIBFV3_FLOAT)
target_include_directories(plugsalot_dragonfly PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shims/dpf
    ${DF}/common
    ${DF}/plugins/dragonfly-plate-reverb
)
if(MSVC)
    target_compile_options(plugsalot_dragonfly PRIVATE /W0 /D_USE_MATH_DEFINES)
else()
    target_compile_options(plugsalot_dragonfly PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# Embedded data: the built catalogue, raw upstream parameter lists, AI guidelines
# ---------------------------------------------------------------------------
set(CAT ${CMAKE_SOURCE_DIR}/catalogue)
# Modules the plugin can run. Catalogue entries without DSP in the plugin yet
# (para_eq, drum_sampler) are left out so the AI never offers them.
set(PLUGSALOT_MODULES console_eq multicomp deess tape plate limiter)
set(PLUGSALOT_DATA_FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/guidelines.md)
foreach(m ${PLUGSALOT_MODULES})
    # Module and raw files share names; copy with prefixes so resource names are unique.
    configure_file(${CAT}/modules/${m}.json ${CMAKE_CURRENT_BINARY_DIR}/data/module_${m}.json COPYONLY)
    configure_file(${CAT}/raw/${m}.json ${CMAKE_CURRENT_BINARY_DIR}/data/raw_${m}.json COPYONLY)
    list(APPEND PLUGSALOT_DATA_FILES
        ${CMAKE_CURRENT_BINARY_DIR}/data/module_${m}.json
        ${CMAKE_CURRENT_BINARY_DIR}/data/raw_${m}.json)
endforeach()
juce_add_binary_data(PlugsalotData
    HEADER_NAME PlugsalotData.h
    NAMESPACE PlugsalotData
    SOURCES ${PLUGSALOT_DATA_FILES}
)

# Engine sources shared by the plugin and the test runner (no GUI dependencies).
set(PLUGSALOT_ENGINE_SOURCES
    src/core/Catalogue.cpp
    src/core/ChainState.cpp
    src/core/ChainValidator.cpp
    src/dsp/ChainEngine.cpp
    src/dsp/ModuleFactory.cpp
    src/dsp/modules/MultiCompModule.cpp
    src/dsp/modules/TapeModule.cpp
    src/dsp/modules/ConsoleEqModule.cpp
    src/dsp/modules/PlateModule.cpp
    src/dsp/modules/LimiterModule.cpp
    src/dsp/modules/DeEssModule.cpp
    src/ai/PromptBuilder.cpp
    src/ai/LlmClient.cpp
    src/ai/AiSession.cpp
)

# ---------------------------------------------------------------------------
# The plugin
# ---------------------------------------------------------------------------
set(PLUGSALOT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGSALOT_FORMATS AU)
endif()
if(UNIX AND NOT APPLE)
    list(APPEND PLUGSALOT_FORMATS LV2)
endif()

juce_add_plugin(Plugsalot
    PRODUCT_NAME "Plugsalot"
    COMPANY_NAME "Plugsalot"
    BUNDLE_ID "org.plugsalot.plugsalot"
    PLUGIN_MANUFACTURER_CODE Plgs
    PLUGIN_CODE Pslt
    FORMATS ${PLUGSALOT_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_CURL TRUE
    NEEDS_WEB_BROWSER FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    LV2URI "https://github.com/adinwalls/plugsalot"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Plugsalot processes live input in the standalone app."
)

target_sources(Plugsalot PRIVATE
    ${PLUGSALOT_ENGINE_SOURCES}
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ui/ChatPanel.cpp
    src/ui/ChainPanel.cpp
    src/ui/ModuleEditor.cpp
    src/ui/ContextBar.cpp
    src/ui/SettingsDialog.cpp
)
target_include_directories(Plugsalot PRIVATE src)

target_compile_definitions(Plugsalot PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    PLUGSALOT_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(Plugsalot
    PRIVATE
        PlugsalotData
        plugsalot_upstream_dsp
        plugsalot_dragonfly
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# Headless test runner: catalogue, validator, every module on test audio
# ---------------------------------------------------------------------------
if(PLUGSALOT_BUILD_TESTS)
    juce_add_console_app(PlugsalotTests PRODUCT_NAME "PlugsalotTests" NEEDS_CURL TRUE)
    target_sources(PlugsalotTests PRIVATE ${PLUGSALOT_ENGINE_SOURCES} tests/TestMain.cpp)
    target_include_directories(PlugsalotTests PRIVATE src)
    target_compile_definitions(PlugsalotTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=1 JUCE_STANDALONE_APPLICATION=1 PLUGSALOT_VERSION="${PROJECT_VERSION}"
        PLUGSALOT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(PlugsalotTests PRIVATE
        PlugsalotData plugsalot_upstream_dsp plugsalot_dragonfly
        juce::juce_core juce::juce_events juce::juce_audio_basics juce::juce_data_structures
        juce::juce_recommended_config_flags)
endif()
