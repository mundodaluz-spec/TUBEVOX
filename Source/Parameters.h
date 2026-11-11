// KingLion Box — parâmetros e presets de fábrica.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/KingLionEngine.h"

namespace klb::ids
{
inline constexpr const char* input    = "input";
inline constexpr const char* hpf      = "hpf";
inline constexpr const char* phase    = "phase";
inline constexpr const char* gain     = "gain";
inline constexpr const char* comp     = "comp";
inline constexpr const char* thr      = "thr";
inline constexpr const char* attack   = "attack";
inline constexpr const char* release  = "release";
inline constexpr const char* link     = "link";
inline constexpr const char* eq       = "eq";
inline constexpr const char* lowF     = "lowf";
inline constexpr const char* lowG     = "lowg";
inline constexpr const char* midF     = "midf";
inline constexpr const char* midG     = "midg";
inline constexpr const char* highF    = "highf";
inline constexpr const char* highG    = "highg";
inline constexpr const char* dsMode   = "dsmode";
inline constexpr const char* dsFreq   = "dsfreq";
inline constexpr const char* dsThr    = "dsthr";
inline constexpr const char* dsListen = "dslisten";
inline constexpr const char* output   = "output";
inline constexpr const char* power    = "power";
inline constexpr const char* meter    = "meter";
}

namespace klb
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Lê os valores atuais (thread de áudio: só leituras atômicas).
struct ParamCache
{
    void attach (juce::AudioProcessorValueTreeState& s);
    Params read() const;

    std::atomic<float>* v[23] {};
};

struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // valores reais (não normalizados)
};
const std::vector<Preset>& factoryPresets();

// Lista com todos os IDs automatizáveis (para salvar/restaurar e testes).
const std::vector<juce::String>& allParamIds();
}
