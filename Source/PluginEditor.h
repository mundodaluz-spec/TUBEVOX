// TUBEVOX — tela (WYATT Sounds). Fundo = a arte original; partes móveis por cima.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/Widgets.h"

class KingLionPanel : public juce::Component
{
public:
    static constexpr int W = 1200, H = 752;

    explicit KingLionPanel (KingLionProcessor&);
    ~KingLionPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override {}

    juce::Component* findControl (const juce::String& paramId);
    klb::ui::VuMeter vu;
    juce::ComboBox presets;
    void updateMeter (float dtSeconds);

private:
    KingLionProcessor& proc;
    klb::ui::LookAndFeel laf;
    klb::ui::Lamp powerLamp;
    juce::OwnedArray<juce::Component> controls;
    float vuValue = -20.0f;
    int lastMeterMode = -1;

    void addKnob (const char* id, klb::ui::Knob::Spec);
    void addSwitch (const char* id, juce::Array<float> values, klb::ui::ToggleSwitch::Spec, float labelWidth);
};

class KingLionEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KingLionEditor (KingLionProcessor&);
    ~KingLionEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override {}
    KingLionPanel& getPanel() { return panel; }

private:
    void timerCallback() override;
    KingLionProcessor& proc;
    KingLionPanel panel;
    double lastTime = 0;
};
