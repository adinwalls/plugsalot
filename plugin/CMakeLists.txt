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

# TONE3000 publishable key (client id). Safe to ship: it only identifies the app. Leave empty
# for builds without TONE3000, or set PLUGSALOT_TONE3000_CLIENT_ID in the environment at run time.
set(PLUGSALOT_TONE3000_CLIENT_ID "" CACHE STRING "TONE3000 publishable key compiled into the plugin")

# Neural Amp Modeler Core, its tone stack, AudioDSPTools and Eigen, compiled unmodified.
# The NAM plugin's ToneStack includes "../AudioDSPTools/dsp/...", so two forwarding headers
# in the build directory point those paths at the vendored copy.
set(NAMDIR ${V}/nam-core)
set(NAM_FWD ${CMAKE_CURRENT_BINARY_DIR}/nam_fwd)
file(MAKE_DIRECTORY ${NAM_FWD}/stub)
file(WRITE ${NAM_FWD}/AudioDSPTools/dsp/dsp.h "#include \"${V}/audiodsptools/dsp/dsp.h\"\n")
file(WRITE ${NAM_FWD}/AudioDSPTools/dsp/RecursiveLinearFilter.h "#include \"${V}/audiodsptools/dsp/RecursiveLinearFilter.h\"\n")
file(GLOB NAM_CORE_SOURCES ${NAMDIR}/NAM/*.cpp ${NAMDIR}/NAM/wavenet/*.cpp)
# OBJECT, not STATIC: NAM registers its architectures with static initialisers that a static
# archive would let the linker drop.
# AudioDSPTools has dsp.cpp and wav.cpp, as does NAM core. Xcode puts every object of a target in one
# folder named after the source, so same-named files collide; compile the AudioDSPTools ones through
# uniquely named wrappers.
foreach(f dsp wav)
    file(WRITE ${NAM_FWD}/adt_${f}.cpp "#include \"${V}/audiodsptools/dsp/${f}.cpp\"\n")
endforeach()
add_library(plugsalot_nam OBJECT
    ${NAM_CORE_SOURCES}
    ${NAM_FWD}/adt_dsp.cpp
    ${NAM_FWD}/adt_wav.cpp
    ${V}/audiodsptools/dsp/NoiseGate.cpp
    ${V}/audiodsptools/dsp/RecursiveLinearFilter.cpp
    ${V}/audiodsptools/dsp/ImpulseResponse.cpp
    ${V}/nam-plugin/NeuralAmpModeler/ToneStack.cpp
)
set_target_properties(plugsalot_nam PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(plugsalot_nam PUBLIC
    ${NAMDIR} ${NAMDIR}/Dependencies/nlohmann ${V}/eigen
    ${V}/audiodsptools ${V}/nam-plugin ${NAM_FWD}/stub
)
if(MSVC)
    target_compile_options(plugsalot_nam PRIVATE /W0 /bigobj /D_USE_MATH_DEFINES)
else()
    target_compile_options(plugsalot_nam PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# Embedded data: the built catalogue, raw upstream parameter lists, AI guidelines
# ---------------------------------------------------------------------------
set(CAT ${CMAKE_SOURCE_DIR}/catalogue)
# Modules the plugin can run. Catalogue entries without DSP in the plugin yet
# (para_eq, drum_sampler) are left out so the AI never offers them.
set(PLUGSALOT_MODULES console_eq multicomp deess tape plate limiter nam_amp cab_ir delay)
set(PLUGSALOT_DATA_FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/guidelines.md)
foreach(m ${PLUGSALOT_MODULES})
    # Module and raw files share names; copy with prefixes so resource names are unique.
    configure_file(${CAT}/modules/${m}.json ${CMAKE_CURRENT_BINARY_DIR}/data/module_${m}.json COPYONLY)
    if(EXISTS ${CAT}/raw/${m}.json)
        configure_file(${CAT}/raw/${m}.json ${CMAKE_CURRENT_BINARY_DIR}/data/raw_${m}.json COPYONLY)
    else() # hand-written (manual) modules have no upstream parameter list
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/data/raw_${m}.json "{\"module\": \"${m}\", \"params\": []}\n")
    endif()
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
    src/core/Library.cpp
    src/core/Tone3000.cpp
    src/dsp/ChainEngine.cpp
    src/dsp/ModuleFactory.cpp
    src/dsp/modules/MultiCompModule.cpp
    src/dsp/modules/TapeModule.cpp
    src/dsp/modules/ConsoleEqModule.cpp
    src/dsp/modules/PlateModule.cpp
    src/dsp/modules/LimiterModule.cpp
    src/dsp/modules/DeEssModule.cpp
    src/dsp/modules/NamAmpModule.cpp
    src/dsp/modules/CabIrModule.cpp
    src/dsp/modules/DelayModule.cpp
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
    COMPANY_NAME "Adin Walls"
    COMPANY_WEBSITE "https://github.com/adinwalls/plugsalot"
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
    src/ui/Tone3000Dialog.cpp
    src/ui/LinuxFocus.cpp
)
target_include_directories(Plugsalot PRIVATE src)

target_compile_definitions(Plugsalot PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    PLUGSALOT_VERSION="${PROJECT_VERSION}"
    PLUGSALOT_TONE3000_CLIENT_ID="${PLUGSALOT_TONE3000_CLIENT_ID}"
)

target_link_libraries(Plugsalot
    PRIVATE
        PlugsalotData
        plugsalot_upstream_dsp
        plugsalot_dragonfly
        plugsalot_nam
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
        PlugsalotData plugsalot_upstream_dsp plugsalot_dragonfly plugsalot_nam
        juce::juce_core juce::juce_events juce::juce_audio_basics juce::juce_data_structures
        juce::juce_recommended_config_flags)
endif()
