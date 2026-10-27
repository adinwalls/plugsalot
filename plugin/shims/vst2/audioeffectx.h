// Plugsalot shim replacing the parts of the VST2 SDK (audioeffectx.h) that
// Airwindows effects use, so their LinuxVST sources compile unmodified as plain
// DSP classes. No Steinberg code is included; this re-declares the small API
// surface Airwindows calls. GPL-3.0-or-later (Plugsalot).
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#define __audioeffect__ 1

typedef int32_t VstInt32;
typedef intptr_t VstIntPtr;
typedef void* audioMasterCallback;

enum VstPlugCategory
{
    kPlugCategUnknown = 0, kPlugCategEffect, kPlugCategSynth, kPlugCategAnalysis, kPlugCategMastering,
    kPlugCategSpacializer, kPlugCategRoomFx, kPlugSurroundFx, kPlugCategRestoration, kPlugCategOfflineProcess,
    kPlugCategShell, kPlugCategGenerator, kPlugCategMaxCount
};

enum
{
    kVstMaxProgNameLen = 24,
    kVstMaxParamStrLen = 8,
    kVstMaxVendorStrLen = 64,
    kVstMaxProductStrLen = 64,
    kVstMaxEffectNameLen = 32
};

inline char* vst_strncpy(char* dst, const char* src, size_t maxLen)
{
    std::strncpy(dst, src, maxLen);
    dst[maxLen] = 0;
    return dst;
}

inline char* vst_strncat(char* dst, const char* src, size_t maxLen)
{
    std::strncat(dst, src, maxLen);
    dst[maxLen] = 0;
    return dst;
}

class AudioEffect
{
public:
    AudioEffect(audioMasterCallback, VstInt32 numPrograms, VstInt32 numParams)
        : numPrograms(numPrograms), numParams(numParams) {}
    virtual ~AudioEffect() = default;

    // Plugsalot drives the sample rate directly.
    void plugsalotSetSampleRate(float sr) { sampleRate = sr; }

    float getSampleRate() const { return sampleRate; }
    virtual void setSampleRate(float sr) { sampleRate = sr; }
    virtual void setBlockSize(VstInt32 size) { blockSize = size; }

    void setNumInputs(VstInt32) {}
    void setNumOutputs(VstInt32) {}
    void setUniqueID(VstInt32) {}
    void canProcessReplacing(bool = true) {}
    void canDoubleReplacing(bool = true) {}
    void programsAreChunks(bool = true) {}
    void isSynth(bool = true) {}
    void noTail(bool = true) {}
    void setInitialDelay(VstInt32) {}

    virtual void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) = 0;
    virtual void processDoubleReplacing(double**, double**, VstInt32) {}
    virtual void setParameter(VstInt32, float) {}
    virtual float getParameter(VstInt32) { return 0.0f; }
    virtual void getParameterLabel(VstInt32, char* text) { text[0] = 0; }
    virtual void getParameterDisplay(VstInt32, char* text) { text[0] = 0; }
    virtual void getParameterName(VstInt32, char* text) { text[0] = 0; }
    virtual VstInt32 getChunk(void**, bool) { return 0; }
    virtual VstInt32 setChunk(void*, VstInt32, bool) { return 0; }
    virtual void getProgramName(char* name) { name[0] = 0; }
    virtual void setProgramName(char*) {}

protected:
    // Display helpers used by Airwindows' getParameterDisplay().
    static void float2string(float value, char* text, VstInt32 maxLen)
    {
        std::snprintf(text, static_cast<size_t>(maxLen) + 1, "%.3f", value);
    }
    static void int2string(VstInt32 value, char* text, VstInt32 maxLen)
    {
        std::snprintf(text, static_cast<size_t>(maxLen) + 1, "%d", value);
    }
    static void dB2string(float value, char* text, VstInt32 maxLen)
    {
        if (value <= 0.0f) vst_strncpy(text, "-oo", static_cast<size_t>(maxLen));
        else std::snprintf(text, static_cast<size_t>(maxLen) + 1, "%.2f", 20.0 * std::log10(value));
    }

    float sampleRate = 44100.0f;
    VstInt32 blockSize = 512;
    VstInt32 numPrograms = 0;
    VstInt32 numParams = 0;
};

class AudioEffectX : public AudioEffect
{
public:
    using AudioEffect::AudioEffect;
    virtual bool getEffectName(char*) { return false; }
    virtual VstPlugCategory getPlugCategory() { return kPlugCategEffect; }
    virtual bool getProductString(char*) { return false; }
    virtual bool getVendorString(char*) { return false; }
    virtual VstInt32 getVendorVersion() { return 0; }
    virtual VstInt32 canDo(char*) { return 0; }
};
