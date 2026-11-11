#include "Widgets.h"
#include "../Parameters.h"
#include <BinaryData.h>

namespace klb::ui
{
using namespace juce;

namespace
{
    Typeface::Ptr load (const void* data, size_t size) { return Typeface::createSystemTypefaceFor (data, size); }
    Typeface::Ptr barlow()  { static auto t = load (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize); return t; }
    Typeface::Ptr barlowB() { static auto t = load (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize); return t; }
    float degToRad (float d) { return d * MathConstants<float>::pi / 180.0f; }
}

Font labelFont (float h) { return Font (FontOptions (barlow()).withHeight (h)); }
Font boldFont (float h)  { return Font (FontOptions (barlowB()).withHeight (h)); }

const Image& skin()
{
    static const Image img = ImageCache::getFromMemory (BinaryData::skin_jpg, BinaryData::skin_jpgSize);
    return img;
}

// ---------- LookAndFeel ----------
LookAndFeel::LookAndFeel()
{
    setColour (ComboBox::textColourId, Colour (0xffffa323));
    setColour (PopupMenu::backgroundColourId, Colour (0xff121110));
    setColour (PopupMenu::textColourId, Colour (0xffffa323));
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff5a3a08));
    setColour (PopupMenu::highlightedTextColourId, Colour (0xffffe0a0));
}

void LookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (10, 0, box.getWidth() - 62, box.getHeight());   // a seta da arte fica à direita
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (Justification::centred);
}

void LookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (Colour (0xff121110));
    g.setColour (Colour (0xff6a4a1a));
    g.drawRect (0, 0, w, h, 1);
}

// ---------- Knob ----------
Knob::Knob (AudioProcessorValueTreeState& s, const String& paramId, Spec spec)
{
    slider.spec = spec;
    slider.setSliderStyle (Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    {
        float a0 = spec.startDeg, a1 = spec.endDeg;
        while (a0 < 0 || a1 < 0) { a0 += 360; a1 += 360; }
        slider.setRotaryParameters (degToRad (a0), degToRad (a1), true);
    }
    slider.setMouseDragSensitivity (220);
    slider.setPopupDisplayEnabled (true, true, nullptr, 900);
    slider.setName (paramId);
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    if (auto* p = s.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
    setName (paramId);
    setInterceptsMouseClicks (false, true);
}

void Knob::resized()
{
    const float r = slider.spec.skinRadius * kSkinScale;
    const auto c = slider.spec.skinCentre * kSkinScale - getPosition().toFloat();
    slider.setBounds (Rectangle<float> (2 * r, 2 * r).withCentre (c).getSmallestIntegerContainer());
}

void Knob::SkinSlider::paint (Graphics& g)
{
    const auto& img = skin();
    const auto rp = getRotaryParameters();
    const double pos = valueToProportionOfLength (getValue());
    const float angleDeg = (float) ((rp.startAngleRadians + pos * (rp.endAngleRadians - rp.startAngleRadians)) * 180.0 / MathConstants<double>::pi);
    float rot = angleDeg - spec.paintedDeg;
    while (rot > 180) rot -= 360;
    while (rot < -180) rot += 360;

    // centro do knob nas coordenadas locais deste slider
    const auto c = spec.skinCentre * kSkinScale - getBoundsInParent().getPosition().toFloat()
                   - getParentComponent()->getPosition().toFloat();
    const float r = spec.skinRadius * kSkinScale;

    Path disc;
    disc.addEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    g.reduceClipRegion (disc);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImageTransformed (img, AffineTransform::translation (-spec.skinCentre.x, -spec.skinCentre.y)
                                     .scaled (kSkinScale)
                                     .rotated (degToRad (rot))
                                     .translated (c.x, c.y));
}

// ---------- chave de alavanca ----------
ToggleSwitch::ToggleSwitch (AudioProcessorValueTreeState& s, const String& paramId, Array<float> v, Spec sp)
    : values (std::move (v)), spec (sp)
{
    auto* param = s.getParameter (paramId);
    jassert (param != nullptr);
    attachment = std::make_unique<ParameterAttachment> (*param, [this] (float val) { setFromParam (val); }, nullptr);
    attachment->sendInitialUpdate();
    setName (paramId);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

Point<float> ToggleSwitch::local (Point<float> skinPt) const
{
    return skinPt * kSkinScale - getPosition().toFloat();
}

void ToggleSwitch::setFromParam (float v)
{
    int best = 0;
    for (int i = 1; i < values.size(); ++i)
        if (std::abs (values[i] - v) < std::abs (values[best] - v)) best = i;
    if (best != position) { position = best; repaint(); }
}

void ToggleSwitch::mouseDown (const MouseEvent& e)
{
    int next = position;
    if (values.size() == 2)
        next = 1 - position;
    else
        next = e.position.y < getHeight() * 0.5f ? jmax (0, position - 1) : jmin (values.size() - 1, position + 1);
    if (next != position)
    {
        position = next;
        attachment->setValueAsCompleteGesture (values[position]);
        repaint();
    }
}

void ToggleSwitch::paint (Graphics& g)
{
    const int n = values.size();
    const bool up = position == 0;
    const bool middle = n == 3 && position == 1;
    if (up && ! spec.sprite)
        return;                                    // a arte já mostra a alavanca para cima

    const auto t = local (spec.targetBushing);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    Graphics::ScopedSaveState ss (g);

    if (middle)
    {
        // alavanca apontando para frente: só a bucha + a ponta cromada vista de frente
        const float br = 22.0f * kSkinScale;
        Path d; d.addEllipse (t.x - br, t.y - br, 2 * br, 2 * br);
        g.reduceClipRegion (d);
        g.drawImageTransformed (skin(), AffineTransform::translation (-spec.skinBushing.x, -spec.skinBushing.y)
                                         .scaled (kSkinScale).translated (t));
        g.setGradientFill (ColourGradient (Colours::white, t.x - 2.5f, t.y - 3, Colour (0xff5f6166), t.x + 4, t.y + 4, true));
        g.fillEllipse (t.x - 5.5f, t.y - 5.5f, 11, 11);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawEllipse (t.x - 5.5f, t.y - 5.5f, 11, 11, 0.6f);
        return;
    }

    const float hw = spec.halfW * kSkinScale, hh = spec.halfH * kSkinScale;
    g.drawImage (patchFor (! up), Rectangle<float> (t.x - hw, t.y - hh, 2 * hw, 2 * hh));
}

Image ToggleSwitch::patchFor (bool flipped)
{
    auto& img = cache[flipped ? 1 : 0];
    if (img.isValid()) return img;
    const int w = (int) std::ceil (2 * spec.halfW), h = (int) std::ceil (2 * spec.halfH);
    img = Image (Image::ARGB, w, h, true);
    {
        Graphics pg (img);
        auto tr = AffineTransform::translation (-spec.skinBushing.x, -spec.skinBushing.y)
                      .scaled (1.0f, flipped ? -1.0f : 1.0f)
                      .translated (spec.halfW, spec.halfH);
        pg.drawImageTransformed (skin(), tr);
    }
    // máscara: só a coluna da alavanca + a bucha (fora disso fica a arte original, com as linhas-guia),
    // com bordas suaves e as pontas esfumadas
    Image::BitmapData bd (img, Image::BitmapData::readWrite);
    const float cx = spec.halfW, cy = spec.halfH;
    const float colHalf = 12.0f, bushR = 21.5f, soft = 3.0f, endFade = 16.0f;
    auto smooth = [] (float a) { a = jlimit (0.0f, 1.0f, a); return a * a * (3 - 2 * a); };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const float px = (float) x + 0.5f, py = (float) y + 0.5f;
            const float col = smooth ((colHalf - std::abs (px - cx)) / soft + 0.5f)
                            * smooth (jmin (py, (float) h - py) / endFade);
            const float bush = smooth ((bushR - std::hypot (px - cx, py - cy)) / soft + 0.5f);
            const float a = jmax (col, bush);
            bd.setPixelColour (x, y, bd.getPixelColour (x, y).withMultipliedAlpha (a));
        }
    return img;
}

// ---------- de-esser ----------
DeEsserControls::DeEsserControls (AudioProcessorValueTreeState& s)
{
    auto* p = s.getParameter (klb::ids::dsMode);
    attachment = std::make_unique<ParameterAttachment> (*p, [this] (float v)
    {
        mode = (int) std::lround (v);
        if (mode != 0) lastActive = mode;
        repaint();
    }, nullptr);
    attachment->sendInitialUpdate();
    setName (klb::ids::dsMode);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void DeEsserControls::mouseDown (const MouseEvent& e)
{
    int next = mode;
    if (buttonArea.expanded (4).contains (e.position) || offLabel.expanded (4).contains (e.position))
        next = mode == 0 ? lastActive : 0;                   // botão OFF: desliga / religa no último modo
    else if (limiterLabel.expanded (6).contains (e.position))
        next = mode == 2 ? 1 : 2;                            // LIMITER: alterna limitador / de-esser
    if (next != mode)
        attachment->setValueAsCompleteGesture ((float) next);
}

void DeEsserControls::paint (Graphics& g)
{
    const Colour panelBlack (0xff1b1b1d);
    // botão: aceso (como na arte) quando a seção está desligada; apagado quando está ativa
    if (mode != 0)
    {
        const auto c = buttonArea.getCentre();
        const float r = buttonArea.getWidth() * 0.30f;
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    }
    auto dim = [&] (Rectangle<float> r) { g.setColour (panelBlack.withAlpha (0.62f)); g.fillRect (r.expanded (2.0f)); };
    if (mode != 0) dim (offLabel);
    if (mode != 2) dim (limiterLabel);
}

// ---------- lâmpada ----------
void Lamp::paint (Graphics& g)
{
    if (on) return;   // a arte já mostra acesa
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = getWidth() * 0.5f;
    g.setGradientFill (ColourGradient (Colour (0xe0200806), c.x, c.y, Colour (0x00200806), c.x + r, c.y, true));
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
}

// ---------- VU ----------
namespace
{
    // escala medida na arte: centro do ponteiro (px 2×) e ângulo de cada marca
    const Point<float> kPivot { 1203.2f, 456.9f };
    const float kDb[]  { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
    const float kAng[] { -35.3f, -24.7f, -18.1f, -11.3f, -4.3f, 1.1f, 6.8f, 12.3f, 20.1f, 27.4f, 34.7f };
    float vuAngle (float db)
    {
        if (db <= kDb[0]) return kAng[0] - jmin (3.0f, (kDb[0] - db) * 0.4f);
        for (int i = 1; i < 11; ++i)
            if (db <= kDb[i])
                return jmap (db, kDb[i - 1], kDb[i], kAng[i - 1], kAng[i]);
        return jmin (kAng[10] + 4.0f, kAng[10] + (db - 3.0f) * 2.0f);
    }
}

void VuMeter::setVu (float v)
{
    v = jlimit (-24.0f, 4.0f, v);
    if (std::abs (v - value) > 0.02f) { value = v; repaint(); }
}

void VuMeter::paint (Graphics& g)
{
    const auto o = getPosition().toFloat();
    const auto pv = kPivot * kSkinScale - o;
    const float a = degToRad (vuAngle (value));
    const Point<float> dir (std::sin (a), -std::cos (a));
    // da borda de baixo da janela (y = 266 na arte) até um pouco além do arco
    const float yBottom = 266.0f * kSkinScale - o.y;
    const float rStart = (pv.y - yBottom) / std::cos (a);
    const float rTip = (306.6f + 16.0f) * kSkinScale;
    const auto p0 = pv + dir * rStart, p1 = pv + dir * rTip;

    g.setColour (Colours::black.withAlpha (0.18f));
    g.drawLine (Line<float> (p0.translated (2.5f, 1.5f), p1.translated (2.5f, 1.5f)), 1.8f);
    g.setColour (Colour (0xff160c03));
    g.drawLine (Line<float> (p0, p1), 1.9f);

    // legenda do modo (onde a arte tinha "L-R")
    g.setColour (Colour (0xff1e1205));
    g.setFont (labelFont (10.5f));
    g.drawText (caption, Rectangle<float> (991.0f * kSkinScale - o.x, 246.0f * kSkinScale - o.y, 40, 10), Justification::centredLeft);
}
} // namespace klb::ui
