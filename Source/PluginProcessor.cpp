#include "PluginProcessor.h"
#include "PluginEditor.h"

KingLionProcessor::KingLionProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",  juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KINGLIONBOX", klb::createLayout())
{
    cache.attach (apvts);
}

bool KingLionProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void KingLionProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setParams (cache.read());
    engine.prepare (sampleRate, juce::jmax (1, juce::jmin (2, getTotalNumOutputChannels())));
}

void KingLionProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nIn = getTotalNumInputChannels(), nOut = getTotalNumOutputChannels();
    for (int c = nIn; c < nOut; ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    engine.setParams (cache.read());
    const int n = juce::jmin (2, buffer.getNumChannels());
    if (n > 0)
        engine.process (buffer.getArrayOfWritePointers(), n, buffer.getNumSamples());
}

float KingLionProcessor::getMeterVu (int mode) const
{
    switch (mode)
    {
        case 0:  return -engine.mCompGr.load();
        case 1:  return engine.mLineIn.load() + 18.0f + 3.01f;   // RMS → nível de pico do seno
        case 2:  return engine.mPreOut.load() + 18.0f + 3.01f;
        case 3:  return engine.mOut.load() + 18.0f + 3.01f;
        default: return -engine.mDsGr.load();
    }
}

// ---------- presets ----------
int KingLionProcessor::getNumPrograms() { return (int) klb::factoryPresets().size(); }

const juce::String KingLionProcessor::getProgramName (int index)
{
    const auto& list = klb::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) list.size()) ? juce::String::fromUTF8 (list[(size_t) index].name) : juce::String();
}

void KingLionProcessor::setCurrentProgram (int index)
{
    const auto& list = klb::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;
    currentProgram = index;
    // começa do padrão e aplica o preset (o medidor escolhido fica como está)
    for (auto& id : klb::allParamIds())
    {
        if (id == klb::ids::meter) continue;
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->getDefaultValue());
    }
    for (auto& [id, value] : list[(size_t) index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

// ---------- estado ----------
void KingLionProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("version", KLB_VERSION, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void KingLionProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    currentProgram = tree.getProperty ("program", 0);
    apvts.replaceState (tree);

    // O APVTS pula parâmetros cujo valor arredondado já bate; força cada um exatamente.
    for (auto child : tree)
    {
        if (! child.hasType ("PARAM")) continue;
        if (auto* p = apvts.getParameter (child["id"].toString()))
        {
            const float v = (float) (double) child["value"];
            const float norm = p->convertTo0to1 (v);
            if (p->getValue() != norm)
                p->setValueNotifyingHost (norm);
        }
    }
}

juce::AudioProcessorEditor* KingLionProcessor::createEditor() { return new KingLionEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KingLionProcessor(); }
