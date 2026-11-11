#include "PluginEditor.h"

using namespace juce;
using klb::ui::Knob;
using klb::ui::ToggleSwitch;
namespace ids = klb::ids;

namespace
{
    constexpr float S = klb::ui::kSkinScale;
    Rectangle<int> fromSkin (float x0, float y0, float x1, float y1)
    {
        return Rectangle<float> (x0 * S, y0 * S, (x1 - x0) * S, (y1 - y0) * S).getSmallestIntegerContainer();
    }
    // posições medidas na arte (px 2×): centro, raio da tampa, ângulo pintado do ponteiro
    const Point<float> phaseBushing { 648, 268 };
}

void KingLionPanel::addKnob (const char* id, Knob::Spec spec)
{
    auto* k = new Knob (proc.apvts, id, spec);
    controls.add (k);
    addAndMakeVisible (k);
    k->setBounds (0, 0, W, H);
}

void KingLionPanel::addSwitch (const char* id, Array<float> values, ToggleSwitch::Spec spec, float labelWidth)
{
    auto* s = new ToggleSwitch (proc.apvts, id, std::move (values), spec);
    controls.add (s);
    addAndMakeVisible (s);
    const auto t = spec.targetBushing;
    s->setBounds (fromSkin (t.x - spec.halfW - 4, t.y - spec.halfH, t.x + spec.halfW + labelWidth / S, t.y + spec.halfH));
}

KingLionPanel::KingLionPanel (KingLionProcessor& p) : proc (p)
{
    setLookAndFeel (&laf);

    // ---------- knobs (recortados da arte e girados) ----------
    addKnob (ids::input,   { { 326.5f, 643.5f }, 85.0f, 46.1f, -135, 135 });
    addKnob (ids::gain,    { { 725.5f, 639.5f }, 74.0f, -88.5f, -135, -45 });
    addKnob (ids::thr,     { { 1909.5f, 445.5f }, 72.0f, -130.3f, -135, 135 });
    addKnob (ids::attack,  { { 1655.5f, 695.5f }, 61.0f, 87.5f, 45, 135 });
    addKnob (ids::release, { { 2174.5f, 698.5f }, 62.0f, -82.7f, -45, -135 });
    addKnob (ids::meter,   { { 1073.5f, 842.5f }, 30.0f, 0.0f, -90, 90 });
    addKnob (ids::lowF,    { { 488.5f, 1126.5f }, 48.5f, -40.7f, -135, 135 });
    addKnob (ids::midF,    { { 795.5f, 1126.5f }, 49.0f, 32.2f, -135, 135 });
    addKnob (ids::highF,   { { 1112.5f, 1128.5f }, 46.5f, 31.4f, -135, 135 });
    addKnob (ids::lowG,    { { 488.5f, 1316.5f }, 51.5f, 0.0f, -135, 135 });
    addKnob (ids::midG,    { { 795.5f, 1315.5f }, 52.5f, 4.0f, -135, 135 });
    addKnob (ids::highG,   { { 1111.5f, 1317.5f }, 50.5f, -2.1f, -135, 135 });
    addKnob (ids::dsFreq,  { { 1778.5f, 1100.5f }, 46.5f, 61.8f, -135, 135 });
    addKnob (ids::dsThr,   { { 1779.5f, 1291.5f }, 55.0f, -124.2f, -135, 135 });
    addKnob (ids::output,  { { 2127.5f, 1205.5f }, 87.0f, 41.3f, -135, 135 });

    // ---------- chaves ----------
    addSwitch (ids::phase, { 0, 1 }, { phaseBushing, 24, 80, phaseBushing, false }, 70);
    addSwitch (ids::comp,  { 1, 0 }, { { 1616, 226 }, 27, 74, { 1616, 226 }, false }, 95);
    addSwitch (ids::link,  { 1, 0 }, { { 2064, 214 }, 25, 82, { 2064, 214 }, false }, 95);
    addSwitch (ids::eq,    { 1, 0 }, { { 162, 1104 }, 25, 72, { 162, 1104 }, false }, 40);
    addSwitch (ids::power, { 1, 0 }, { { 1370, 850 }, 27, 70, { 1370, 850 }, false }, 30);
    // HPF e MONITOR: a arte tinha alavancas tortas; o lugar foi limpo e usa a alavanca da PHASE
    addSwitch (ids::hpf,      { 2, 1, 0 }, { phaseBushing, 22, 80, { 200, 260 }, true }, 60);
    addSwitch (ids::dsListen, { 1, 0 },    { phaseBushing, 22, 80, { 1406, 1287 }, true }, 80);

    {
        auto* ds = new klb::ui::DeEsserControls (proc.apvts);
        controls.add (ds);
        addAndMakeVisible (ds);
        const auto b = fromSkin (1370, 1064, 1566, 1178);
        ds->setBounds (b);
        const auto o = b.getPosition().toFloat();
        ds->buttonArea   = Rectangle<float> (1380 * S, 1072 * S, 44 * S, 44 * S) - o;
        ds->offLabel     = Rectangle<float> (1434 * S, 1078 * S, 66 * S, 24 * S) - o;
        ds->limiterLabel = Rectangle<float> (1455 * S, 1146 * S, 102 * S, 23 * S) - o;
    }

    // ---------- centro ----------
    addAndMakeVisible (vu);
    vu.setBounds (fromSkin (930, 40, 1480, 280));
    addAndMakeVisible (powerLamp);
    powerLamp.setBounds (Rectangle<int> (34, 34).withCentre ({ 640, 401 }));

    addAndMakeVisible (presets);
    presets.setBounds (fromSkin (944, 698, 1456, 750));
    presets.setTextWhenNothingSelected ("PRESETS");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presets.addItem (proc.getProgramName (i), i + 1);
    presets.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    presets.onChange = [this]
    {
        if (presets.getSelectedId() > 0)
            proc.setCurrentProgram (presets.getSelectedId() - 1);
    };

    setSize (W, H);
}

KingLionPanel::~KingLionPanel()
{
    presets.hidePopup();
    setLookAndFeel (nullptr);
}

Component* KingLionPanel::findControl (const String& paramId)
{
    for (auto* c : controls)
        if (c->getName() == paramId) return c;
    return nullptr;
}

void KingLionPanel::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (klb::ui::skin(), getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

void KingLionPanel::updateMeter (float dt)
{
    const int mode = (int) std::lround (proc.apvts.getRawParameterValue (ids::meter)->load());
    const float target = proc.getMeterVu (mode);
    const float k = 1.0f - std::exp (-dt / 0.065f);   // balística de VU
    vuValue += (target - vuValue) * k;
    vu.setVu (vuValue);
    if (mode != lastMeterMode)
    {
        static const char* names[] = { "G-R", "IN", "PRE", "OUT", "D-S" };
        vu.setCaption (names[jlimit (0, 4, mode)]);
        lastMeterMode = mode;
    }
    powerLamp.setOn (proc.apvts.getRawParameterValue (ids::power)->load() > 0.5f);
    const int prog = proc.getCurrentProgram() + 1;
    if (presets.getSelectedId() != prog && ! presets.isPopupActive())
        presets.setSelectedId (prog, dontSendNotification);
}

// ---------- editor ----------
KingLionEditor::KingLionEditor (KingLionProcessor& p) : AudioProcessorEditor (p), proc (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits ((int) (KingLionPanel::W * 0.7), (int) (KingLionPanel::H * 0.7),
                     KingLionPanel::W * 2, KingLionPanel::H * 2);
    getConstrainer()->setFixedAspectRatio ((double) KingLionPanel::W / KingLionPanel::H);
    setSize (KingLionPanel::W, KingLionPanel::H);
    lastTime = Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

KingLionEditor::~KingLionEditor() { stopTimer(); }

void KingLionEditor::resized()
{
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) KingLionPanel::W));
}

void KingLionEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const float dt = (float) jlimit (0.001, 0.2, (now - lastTime) * 0.001);
    lastTime = now;
    panel.updateMeter (dt);
}
