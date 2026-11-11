// TUBEVOX — processador (WYATT Sounds).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

class KingLionProcessor : public juce::AudioProcessor
{
public:
    KingLionProcessor();
    ~KingLionProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TUBEVOX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Medidor: valor em "VU" para o modo escolhido (0 VU = −18 dBFS; G-R e D-S: −redução).
    float getMeterVu (int mode) const;

    juce::AudioProcessorValueTreeState apvts;

private:
    klb::Engine engine;
    klb::ParamCache cache;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KingLionProcessor)
};
