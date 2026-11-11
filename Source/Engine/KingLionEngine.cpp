#include "KingLionEngine.h"

namespace klb
{
namespace
{
    // Voicing das válvulas (calibrado para 0 dBFS ≈ +20 dBu de nível de linha).
    // Alvo: limpo em nível nominal (−18 dBFS), cor de ~1% perto de 0 dBFS, 2º harmônico na frente
    // quando o pré é empurrado (GAIN 55/60 ou ENTRADA alta).
    constexpr double kTriodeDrive = 0.22, kTriodeBias = 0.10;   // 12AX7: assimetria → 2º harmônico
    constexpr double kFollowDrive = 0.18, kFollowBias = 0.0;    // seguidor de catodo 6414: simétrico
    constexpr double kLineDrive   = 0.15, kLineBias   = 0.05;   // estágio de linha do EQ (5751)
    constexpr double kXfmrSat     = 0.9;                        // saturação de graves do transformador
    constexpr double kCompKnee = 6.0, kCompRatio = 3.0;
    constexpr double kDsRatio = 6.0, kDsKnee = 4.0, kDsMax = 20.0;
    constexpr double kLimRatio = 10.0, kLimKnee = 2.0;
    constexpr double kCompMaxStart = 12.0, kCompMaxSpan = 6.0;  // redução satura em ~18 dB

    inline double softMax (double gr)
    {
        if (gr <= kCompMaxStart) return gr;
        return kCompMaxStart + kCompMaxSpan * std::tanh ((gr - kCompMaxStart) / kCompMaxSpan);
    }
}

void Engine::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    nCh = std::clamp (numChannels, 1, kMaxCh);
    smC = timeCoef (sr, 20.0);
    mixStep = 1.0 / (0.020 * sr);
    detRmsC = timeCoef (sr, 8.0);
    dsEnvAttC = timeCoef (sr, 0.3);
    dsEnvRelC = timeCoef (sr, 25.0);

    for (auto& c : ch)
    {
        c.tube1.setup (kTriodeDrive, kTriodeBias);
        c.tube2.setup (kFollowDrive, kFollowBias);
        c.tube3.setup (kLineDrive, kLineBias);
        c.dc1.setup (sr, 4.0);
        c.dc3.setup (sr, 4.0);
        c.xfmrLp.setup (sr, 90.0);
        c.xfmrBump.setPeak (sr, 28.0, 0.8, 0.6);
        c.xfmrHp.setHighpass (sr, 7.0, 0.7071);
    }
    lastHpf = lastLowF = lastMidF = lastHighF = lastDsF = lastDsMode = lastAtt = lastRel = -1;
    prepared = true;
    reset();
    updateFilters (true);
}

void Engine::reset()
{
    for (auto& c : ch)
    {
        c.hpf1.reset(); c.hpf2.reset();
        c.rms = 0; c.opto.reset();
        c.tube1.reset(); c.tube2.reset(); c.tube3.reset();
        c.dc1.reset(); c.dc3.reset();
        c.xfmrLp.z = 0; c.xfmrBump.reset(); c.xfmrHp.reset();
        c.low.reset(); c.mid.reset(); c.high.reset();
        c.dsBand.reset(); c.dsEnv = 0; c.dsOpto.reset();
    }
    inG = inGT = dbToGain (p.inputDb);
    outG = outGT = dbToGain (p.outputDb);
    drv = drvT = dbToGain (kGainSteps[(size_t) std::clamp (p.gainStep, 0, 4)] - 50.0);
    mix = mixT = p.power ? 1.0 : 0.0;
    lowGs = p.lowGain; midGs = p.midGain; highGs = p.highGain;
    mCompGr = 0; mDsGr = 0;
}

void Engine::setParams (const Params& np)
{
    p = np;
    inGT  = dbToGain (p.inputDb);
    outGT = dbToGain (p.outputDb);
    drvT  = dbToGain (kGainSteps[(size_t) std::clamp (p.gainStep, 0, 4)] - 50.0);
    mixT  = p.power ? 1.0 : 0.0;
    if (prepared) updateFilters (false);
}

void Engine::updateFilters (bool force)
{
    const double nyq = 0.45 * sr;
    if (force || p.hpf != lastHpf)
    {
        lastHpf = p.hpf;
        for (auto& c : ch)
        {
            if (p.hpf == 0) { c.hpf1.setIdentity(); c.hpf2.setIdentity(); }
            else
            {
                const double f = p.hpf == 1 ? 80.0 : 120.0;
                c.hpf1.setHighpass (sr, f, 0.7071);   // 12 dB/oit (Butterworth)
                c.hpf2.setIdentity();
            }
        }
    }
    if (force || p.attack != lastAtt || p.release != lastRel)
    {
        lastAtt = p.attack; lastRel = p.release;
        for (auto& c : ch)
            c.opto.setup (sr, kAttackMs[(size_t) std::clamp (p.attack, 0, 4)],
                              kReleaseMs[(size_t) std::clamp (p.release, 0, 4)]);
    }
    if (force || p.dsFreq != lastDsF || p.dsMode != lastDsMode)
    {
        lastDsF = p.dsFreq; lastDsMode = p.dsMode;
        const double f = std::min ((double) kDsFreqs[(size_t) std::clamp (p.dsFreq, 0, 3)], nyq);
        for (auto& c : ch)
        {
            c.dsBand.setBandpass (sr, f, 1.2);
            if (p.dsMode == 2) c.dsOpto.setup (sr, 1.0, 300.0);   // limitador óptico
            else               c.dsOpto.setup (sr, 1.0, 60.0);    // de-esser: solta rápido
        }
    }

    const bool freqChanged = p.lowFreq != lastLowF || p.midFreq != lastMidF || p.highFreq != lastHighF;
    const bool gainMoving = std::fabs (lowGs - p.lowGain) > 1e-4 || std::fabs (midGs - p.midGain) > 1e-4
                         || std::fabs (highGs - p.highGain) > 1e-4;
    if (force || freqChanged || gainMoving)
    {
        lastLowF = p.lowFreq; lastMidF = p.midFreq; lastHighF = p.highFreq;
        if (force) { lowGs = p.lowGain; midGs = p.midGain; highGs = p.highGain; }
        const double fl = kLowFreqs[(size_t) std::clamp (p.lowFreq, 0, 3)];
        const double fm = std::min ((double) kMidFreqs[(size_t) std::clamp (p.midFreq, 0, 7)], nyq);
        const double fh = std::min ((double) kHighFreqs[(size_t) std::clamp (p.highFreq, 0, 3)], nyq);
        for (auto& c : ch)
        {
            c.low.setPeak (sr, fl, 0.65, lowGs);
            c.mid.setPeak (sr, fm, 1.0, midGs);
            c.high.setPeak (sr, fh, 0.6, highGs);
        }
    }
}

void Engine::process (float* const* data, int numChannels, int numSamples)
{
    const int nc = std::min (numChannels, kMaxCh);

    // Totalmente desligado: saída = entrada, bit a bit.
    if (mix <= 0.0 && mixT <= 0.0)
    {
        mCompGr = 0; mDsGr = 0;
        return;
    }

    const double cThr = compThresholdDb (p.compThr);
    const double dThr = dsThresholdDb (p.dsThr);
    const double eqC = timeCoef (sr, 15.0);

    double sLine = 0, sPre = 0, sEq = 0, sOut = 0, maxGr = 0, maxDs = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        if (++coefCounter >= 32)
        {
            coefCounter = 0;
            const double k32 = std::pow (eqC, 32.0);
            lowGs  = p.lowGain  + k32 * (lowGs  - p.lowGain);
            midGs  = p.midGain  + k32 * (midGs  - p.midGain);
            highGs = p.highGain + k32 * (highGs - p.highGain);
            updateFilters (false);
        }

        inG  = inGT  + smC * (inG  - inGT);
        outG = outGT + smC * (outG - outGT);
        drv  = drvT  + smC * (drv  - drvT);
        if (mix < mixT) mix = std::min (mixT, mix + mixStep);
        else if (mix > mixT) mix = std::max (mixT, mix - mixStep);

        double x[kMaxCh], dry[kMaxCh], grC[kMaxCh] = { 0, 0 };

        // 1) entrada, fase, corte de graves e detector do compressor
        for (int c = 0; c < nc; ++c)
        {
            auto& s = ch[(size_t) c];
            dry[c] = data[c][i];
            double v = dry[c] * inG;
            if (p.phase180) v = -v;
            v = s.hpf1.process (v);
            x[c] = v;
            sLine += v * v;
            s.rms = v * v + detRmsC * (s.rms - v * v);
            if (p.compOn)
            {
                const double lvl = 10.0 * std::log10 (std::max (s.rms * 2.0, 1e-20)); // pico equivalente do seno
                grC[c] = softMax (kneeReduction (lvl - cThr, kCompRatio, kCompKnee));
            }
        }
        if (p.link && nc == 2) grC[0] = grC[1] = std::max (grC[0], grC[1]);

        double grDs[kMaxCh] = { 0, 0 }, band[kMaxCh] = { 0, 0 };

        for (int c = 0; c < nc; ++c)
        {
            auto& s = ch[(size_t) c];
            double v = x[c];

            // 2) célula óptica antes da válvula
            const double gr = s.opto.process (p.compOn ? grC[c] : 0.0);
            maxGr = std::max (maxGr, gr);
            v *= dbToGain (-gr);

            // 3) pré: triodo assimétrico → seguidor de catodo → transformador
            v = s.tube1.process (v * drv);
            v = s.dc1.process (v);
            v = s.tube2.process (v);
            {
                const double lp = s.xfmrLp.process (v);
                v += std::tanh (lp * kXfmrSat) / kXfmrSat - lp;
                v = s.xfmrBump.process (v);
                v = s.xfmrHp.process (v);
            }
            sPre += v * v;

            // 4) EQ pico-vale-pico + estágio de linha
            if (p.eqOn)
            {
                v = s.low.process (v);
                v = s.mid.process (v);
                v = s.high.process (v);
            }
            v = s.tube3.process (v);
            v = s.dc3.process (v);
            sEq += v * v;
            x[c] = v;

            // 5) detector do de-esser / limitador
            band[c] = s.dsBand.process (v);
            if (p.dsMode != 0)
            {
                const double det = p.dsMode == 1 ? std::fabs (band[c]) : std::fabs (v);
                const double ec = det > s.dsEnv ? dsEnvAttC : dsEnvRelC;
                s.dsEnv = det + ec * (s.dsEnv - det);
                const double lvl = gainToDb (s.dsEnv);
                grDs[c] = p.dsMode == 1 ? std::min (kDsMax, kneeReduction (lvl - dThr, kDsRatio, kDsKnee))
                                        : kneeReduction (lvl - dThr, kLimRatio, kLimKnee);
            }
        }
        if (p.link && nc == 2) grDs[0] = grDs[1] = std::max (grDs[0], grDs[1]);

        for (int c = 0; c < nc; ++c)
        {
            auto& s = ch[(size_t) c];
            double v = x[c];
            const double g = dbToGain (-s.dsOpto.process (grDs[c]));
            maxDs = std::max (maxDs, -gainToDb (g));
            if (p.dsMode == 1)      v = p.dsListen ? band[c] : v + (g - 1.0) * band[c];
            else if (p.dsMode == 2) v *= g;

            v *= outG;
            sOut += v * v;
            data[c][i] = (float) (dry[c] + mix * (v - dry[c]));
        }
    }

    const double n = (double) std::max (1, numSamples * nc);
    auto toDb = [n] (double s) { return (float) (10.0 * std::log10 (std::max (s / n, 1e-20))); };
    mLineIn = toDb (sLine); mPreOut = toDb (sPre); mEqOut = toDb (sEq); mOut = toDb (sOut);
    mCompGr = (float) maxGr; mDsGr = (float) maxDs;
}
} // namespace klb
