// KingLion Box — interface feita sobre a arte original (skin.jpg, 2400×1504 = 2× o painel).
// As partes móveis são recortadas da própria arte: knobs giram, alavancas espelham, VU e preset são desenhados por cima.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace klb::ui
{
const juce::Image& skin();                 // arte em 2× (cache)
constexpr float kSkinScale = 0.5f;         // pixels da arte → pixels do painel

juce::Font labelFont (float h);
juce::Font boldFont (float h);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override {}  // a caixa já está na arte
    juce::Font getComboBoxFont (juce::ComboBox&) override { return boldFont (18.0f); }
    juce::Font getPopupMenuFont() override { return labelFont (17.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
};

// ---------- knob: o próprio knob da arte, girado ----------
class Knob : public juce::Component
{
public:
    struct Spec
    {
        juce::Point<float> skinCentre;   // centro na arte (px 2×)
        float skinRadius;                // raio da tampa que gira (px 2×)
        float paintedDeg;                // para onde o ponteiro aponta na arte (0 = 12 h, horário)
        float startDeg, endDeg;          // curso do knob
    };
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, Spec);
    void resized() override;
    juce::Slider& getSlider() { return slider; }

private:
    struct SkinSlider : juce::Slider
    {
        Spec spec;
        void paint (juce::Graphics&) override;
    } slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ---------- chave de alavanca ----------
// Na arte a alavanca está para cima. "Para baixo" = a mesma janela espelhada na vertical em volta da bucha.
// Também serve para HPF e MONITOR: aí a alavanca vem da chave PHASE (sprite) e é colocada no lugar.
class ToggleSwitch : public juce::Component
{
public:
    struct Spec
    {
        juce::Point<float> skinBushing;      // bucha da alavanca usada (px 2×)
        float halfW, halfH;                  // janela da alavanca (px 2×)
        juce::Point<float> targetBushing;    // onde desenhar (igual a skinBushing se for a própria)
        bool sprite = false;                 // true = sempre desenha a alavanca (o lugar foi limpo na arte)
    };
    // values: de cima para baixo. 2 posições = cima/baixo; 3 posições = cima/meio (aponta para frente)/baixo.
    ToggleSwitch (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::Array<float> values, Spec);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int getLeverPosition() const { return position; }

private:
    void setFromParam (float v);
    juce::Point<float> local (juce::Point<float> skinPt) const;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::Image patchFor (bool flipped);     // alavanca recortada, com bordas esfumadas (cache)
    juce::Array<float> values;
    Spec spec;
    int position = 0;
    juce::Image cache[2];
};

// ---------- botão do de-esser (OFF) + rótulo LIMITER ----------
class DeEsserControls : public juce::Component
{
public:
    DeEsserControls (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void resized() override {}
    juce::Rectangle<float> buttonArea, offLabel, limiterLabel;   // coordenadas locais (painel)

private:
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int mode = 0, lastActive = 1;
};

class Lamp : public juce::Component
{
public:
    void setOn (bool b) { if (b != on) { on = b; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    bool on = true;
};

// ---------- agulha e legenda do VU ----------
class VuMeter : public juce::Component
{
public:
    VuMeter() { setInterceptsMouseClicks (false, false); }
    void setVu (float vuDb);
    void setCaption (const juce::String& s) { if (s != caption) { caption = s; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    float value = -20.0f;
    juce::String caption { "G-R" };
};
} // namespace klb::ui
