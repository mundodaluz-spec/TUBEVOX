// KingLion Box — motor de áudio (WYATT Sounds). Sem interface e sem JUCE.
//
// Cadeia (por canal):
//   ENTRADA → FASE → CORTE DE GRAVES (80/120) → COMPRESSOR ÓPTICO 3:1 →
//   PRÉ VALVULADO (triodo assimétrico + seguidor de catodo + transformador) →
//   EQ PICO-VALE-PICO → ESTÁGIO DE LINHA VALVULADO → DE-ESSER / LIMITADOR ÓPTICO → SAÍDA
#pragma once
#include "Dsp.h"
#include <atomic>
#include <array>

namespace klb
{
struct Params
{
    float inputDb   = 0.0f;   // −24…+12
    int   hpf       = 0;      // 0 FLAT, 1 = 80 Hz, 2 = 120 Hz
    bool  phase180  = false;
    int   gainStep  = 2;      // 0..4 → 40/45/50/55/60 dB (50 = ganho unitário em sinal baixo)

    bool  compOn    = false;
    float compThr   = 0.0f;   // 0..10 (0 = sem compressão, 10 = −40 dBFS)
    int   attack    = 2;      // 0 FAST … 4 SLOW
    int   release   = 2;      // 0 FAST … 4 SLOW
    bool  link      = true;

    bool  eqOn      = true;
    int   lowFreq   = 1;  float lowGain  = 0.0f;   // ±10 dB
    int   midFreq   = 4;  float midGain  = 0.0f;
    int   highFreq  = 1;  float highGain = 0.0f;

    int   dsMode    = 0;      // 0 DESLIGADO, 1 DE-ESSER, 2 LIMITADOR
    int   dsFreq    = 1;      // 3/6/9/12 kHz
    float dsThr     = 0.0f;   // 0..10
    bool  dsListen  = false;

    float outputDb  = 0.0f;   // −24…+12
    bool  power     = true;
};

// Tabelas usadas pela interface e pelo motor.
constexpr std::array<float, 5> kGainSteps   { 40, 45, 50, 55, 60 };
constexpr std::array<float, 5> kAttackMs    { 4, 10, 20, 40, 70 };
constexpr std::array<float, 5> kReleaseMs   { 60, 150, 300, 600, 1200 };
constexpr std::array<float, 4> kLowFreqs    { 35, 60, 110, 220 };
constexpr std::array<float, 8> kMidFreqs    { 200, 350, 500, 800, 1200, 2000, 3500, 5000 };
constexpr std::array<float, 4> kHighFreqs   { 5000, 8000, 12000, 16000 };
constexpr std::array<float, 4> kDsFreqs     { 3000, 6000, 9000, 12000 };

inline float compThresholdDb (float knob) { return -4.0f * knob; }
inline float dsThresholdDb   (float knob) { return -4.0f * knob; }

class Engine
{
public:
    static constexpr int kMaxCh = 2;

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setParams (const Params& p);                 // pode ser chamado a cada bloco
    void process (float* const* data, int numChannels, int numSamples);

    // Medidores (dB). Atualizados a cada bloco.
    std::atomic<float> mLineIn { -100 }, mPreOut { -100 }, mEqOut { -100 }, mOut { -100 };
    std::atomic<float> mCompGr { 0 }, mDsGr { 0 };

    double getSampleRate() const { return sr; }

private:
    void updateFilters (bool force);

    double sr = 48000;
    int nCh = 2;
    Params p;

    struct Chan
    {
        Biquad hpf1, hpf2;
        double rms = 0;
        OptoCell opto;
        TubeStage tube1, tube2, tube3;
        DcBlock dc1, dc3;
        OnePoleLP xfmrLp;
        Biquad xfmrBump, xfmrHp;
        Biquad low, mid, high;
        Biquad dsBand;
        double dsEnv = 0;
        OptoCell dsOpto;
    };
    std::array<Chan, kMaxCh> ch;

    // suavização
    double inG = 1, inGT = 1, outG = 1, outGT = 1, drv = 1, drvT = 1, mix = 1, mixT = 1;
    double smC = 0, mixStep = 0;
    double lowGs = 0, midGs = 0, highGs = 0;  // ganhos de EQ suavizados
    double detRmsC = 0, dsEnvRelC = 0, dsEnvAttC = 0;
    int coefCounter = 0;
    int lastHpf = -1, lastLowF = -1, lastMidF = -1, lastHighF = -1, lastDsF = -1, lastDsMode = -1;
    int lastAtt = -1, lastRel = -1;
    bool prepared = false;
};
} // namespace klb
