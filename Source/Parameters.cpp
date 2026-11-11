#include "Parameters.h"

namespace klb
{
using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using PID = juce::ParameterID;

namespace
{
    juce::NormalisableRange<float> range (float lo, float hi, float step = 0.01f) { return { lo, hi, step }; }

    juce::AudioParameterFloatAttributes dbAttr()
    {
        return juce::AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; });
    }
    juce::AudioParameterFloatAttributes dialAttr()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); });
    }
    const juce::StringArray speeds { "FAST", "MED FAST", "MEDIUM", "MED SLOW", "SLOW" };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    const int v = 1;
    p.push_back (std::make_unique<APF> (PID { ids::input, v },  "Input", range (-24, 12), 0.0f, dbAttr()));
    p.push_back (std::make_unique<APC> (PID { ids::hpf, v },    "High-pass filter", juce::StringArray { "FLAT", "80 Hz", "120 Hz" }, 0));
    p.push_back (std::make_unique<APB> (PID { ids::phase, v },  "Phase 180", false));
    p.push_back (std::make_unique<APC> (PID { ids::gain, v },   "Preamp gain", juce::StringArray { "40 dB", "45 dB", "50 dB", "55 dB", "60 dB" }, 2));
    p.push_back (std::make_unique<APB> (PID { ids::comp, v },   "Compressor 3:1", true));
    p.push_back (std::make_unique<APF> (PID { ids::thr, v },    "Threshold", range (0, 10), 0.0f, dialAttr()));
    p.push_back (std::make_unique<APC> (PID { ids::attack, v }, "Attack", speeds, 2));
    p.push_back (std::make_unique<APC> (PID { ids::release, v },"Release", speeds, 2));
    p.push_back (std::make_unique<APB> (PID { ids::link, v },   "Link", true));
    p.push_back (std::make_unique<APB> (PID { ids::eq, v },     "EQ on", true));
    p.push_back (std::make_unique<APC> (PID { ids::lowF, v },   "Low freq", juce::StringArray { "35 Hz", "60 Hz", "110 Hz", "220 Hz" }, 1));
    p.push_back (std::make_unique<APF> (PID { ids::lowG, v },   "Low gain", range (-10, 10, 0.1f), 0.0f, dbAttr()));
    p.push_back (std::make_unique<APC> (PID { ids::midF, v },   "Mid freq", juce::StringArray { "200 Hz", "350 Hz", "500 Hz", "800 Hz", "1.2 kHz", "2 kHz", "3.5 kHz", "5 kHz" }, 4));
    p.push_back (std::make_unique<APF> (PID { ids::midG, v },   "Mid gain", range (-10, 10, 0.1f), 0.0f, dbAttr()));
    p.push_back (std::make_unique<APC> (PID { ids::highF, v },  "High freq", juce::StringArray { "5 kHz", "8 kHz", "12 kHz", "16 kHz" }, 2));
    p.push_back (std::make_unique<APF> (PID { ids::highG, v },  "High gain", range (-10, 10, 0.1f), 0.0f, dbAttr()));
    p.push_back (std::make_unique<APC> (PID { ids::dsMode, v }, "De-esser mode", juce::StringArray { "OFF", "DE-ESSER", "LIMITER" }, 0));
    p.push_back (std::make_unique<APC> (PID { ids::dsFreq, v }, "De-esser freq", juce::StringArray { "3 kHz", "6 kHz", "9 kHz", "12 kHz" }, 1));
    p.push_back (std::make_unique<APF> (PID { ids::dsThr, v },  "De-esser threshold", range (0, 10), 0.0f, dialAttr()));
    p.push_back (std::make_unique<APB> (PID { ids::dsListen, v },"Monitor", false));
    p.push_back (std::make_unique<APF> (PID { ids::output, v }, "Output", range (-24, 12), 0.0f, dbAttr()));
    p.push_back (std::make_unique<APB> (PID { ids::power, v },  "Power", true));
    p.push_back (std::make_unique<APC> (PID { ids::meter, v },  "Meter", juce::StringArray { "G-R", "IN", "PRE", "OUT", "D-S" }, 0,
                                         juce::AudioParameterChoiceAttributes().withAutomatable (false)));
    return { p.begin(), p.end() };
}

const std::vector<juce::String>& allParamIds()
{
    static const std::vector<juce::String> v {
        ids::input, ids::hpf, ids::phase, ids::gain, ids::comp, ids::thr, ids::attack, ids::release, ids::link,
        ids::eq, ids::lowF, ids::lowG, ids::midF, ids::midG, ids::highF, ids::highG,
        ids::dsMode, ids::dsFreq, ids::dsThr, ids::dsListen, ids::output, ids::power, ids::meter };
    return v;
}

void ParamCache::attach (juce::AudioProcessorValueTreeState& s)
{
    const auto& idsv = allParamIds();
    for (size_t i = 0; i < idsv.size(); ++i)
        v[i] = s.getRawParameterValue (idsv[i]);
}

Params ParamCache::read() const
{
    auto f = [this] (int i) { return v[i]->load(); };
    auto n = [this] (int i) { return (int) std::lround (v[i]->load()); };
    auto b = [this] (int i) { return v[i]->load() > 0.5f; };
    Params p;
    p.inputDb = f (0);  p.hpf = n (1);  p.phase180 = b (2);  p.gainStep = n (3);
    p.compOn = b (4);   p.compThr = f (5); p.attack = n (6); p.release = n (7); p.link = b (8);
    p.eqOn = b (9);     p.lowFreq = n (10); p.lowGain = f (11); p.midFreq = n (12); p.midGain = f (13);
    p.highFreq = n (14); p.highGain = f (15);
    p.dsMode = n (16);  p.dsFreq = n (17); p.dsThr = f (18); p.dsListen = b (19);
    p.outputDb = f (20); p.power = b (21);
    return p;
}

const std::vector<Preset>& factoryPresets()
{
    // índices: hpf 0 FLAT/1 80/2 120 · gain 0..4 = 40..60 · attack/release 0 FAST..4 SLOW
    // lowf 35/60/110/220 · midf 200/350/500/800/1.2k/2k/3.5k/5k · highf 5k/8k/12k/16k
    // dsmode 0 desl/1 de-esser/2 limitador · dsfreq 3/6/9/12 kHz
    static const std::vector<Preset> list {
        { "Default", {} },
        { "Gospel Lead Vocal", { { ids::hpf, 1 }, { ids::comp, 1 }, { ids::thr, 4.5f }, { ids::attack, 2 }, { ids::release, 2 },
                               { ids::lowF, 2 }, { ids::lowG, 1.5f }, { ids::midF, 1 }, { ids::midG, -2 },
                               { ids::highF, 2 }, { ids::highG, 4 }, { ids::dsMode, 1 }, { ids::dsFreq, 1 }, { ids::dsThr, 5 } } },
        { "Rap Vocal Upfront", { { ids::input, 3 }, { ids::hpf, 2 }, { ids::gain, 3 }, { ids::comp, 1 }, { ids::thr, 6 },
                                 { ids::attack, 1 }, { ids::release, 1 }, { ids::midF, 5 }, { ids::midG, 2.5f },
                                 { ids::highF, 1 }, { ids::highG, 3 }, { ids::dsMode, 1 }, { ids::dsFreq, 1 }, { ids::dsThr, 6 },
                                 { ids::output, -6 } } },
        { "Soft Backing Vocals", { { ids::hpf, 2 }, { ids::comp, 1 }, { ids::thr, 5 }, { ids::attack, 3 }, { ids::release, 3 },
                            { ids::midF, 4 }, { ids::midG, -2 }, { ids::highF, 2 }, { ids::highG, 2 },
                            { ids::dsMode, 1 }, { ids::dsFreq, 1 }, { ids::dsThr, 4 } } },
        { "Speech & Sermon", { { ids::hpf, 1 }, { ids::comp, 1 }, { ids::thr, 6 }, { ids::attack, 2 }, { ids::release, 2 },
                                  { ids::lowF, 2 }, { ids::lowG, 2 }, { ids::midF, 1 }, { ids::midG, -3 },
                                  { ids::highF, 1 }, { ids::highG, 2 }, { ids::dsMode, 1 }, { ids::dsFreq, 1 }, { ids::dsThr, 5 } } },
        { "Acoustic Guitar", { { ids::hpf, 1 }, { ids::comp, 1 }, { ids::thr, 3 }, { ids::attack, 3 }, { ids::release, 2 },
                      { ids::midF, 2 }, { ids::midG, -2 }, { ids::highF, 2 }, { ids::highG, 3 } } },
        { "Fat Bass", { { ids::gain, 3 }, { ids::comp, 1 }, { ids::thr, 5 }, { ids::attack, 2 }, { ids::release, 3 },
                           { ids::lowF, 1 }, { ids::lowG, 3 }, { ids::midF, 3 }, { ids::midG, 2 },
                           { ids::highF, 0 }, { ids::highG, -2 }, { ids::dsMode, 2 }, { ids::dsThr, 2 }, { ids::output, -4 } } },
        { "Punchy Snare", { { ids::gain, 3 }, { ids::comp, 1 }, { ids::thr, 6 }, { ids::attack, 2 }, { ids::release, 0 },
                             { ids::lowF, 3 }, { ids::lowG, 3 }, { ids::midF, 1 }, { ids::midG, -3 },
                             { ids::highF, 0 }, { ids::highG, 3 }, { ids::dsMode, 2 }, { ids::dsThr, 3 }, { ids::output, -5 } } },
        { "Lion Roar", { { ids::input, 6 }, { ids::gain, 4 }, { ids::comp, 1 }, { ids::thr, 4 }, { ids::attack, 0 },
                              { ids::release, 1 }, { ids::midF, 5 }, { ids::midG, 3 }, { ids::highF, 1 }, { ids::highG, 2 },
                              { ids::dsMode, 2 }, { ids::dsThr, 1 }, { ids::output, -14 } } },
    };
    return list;
}
} // namespace klb
