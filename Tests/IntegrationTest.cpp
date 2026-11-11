// TUBEVOX — teste de integração do plugin (processador + tela, mouse simulado).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

using namespace juce;

static int failures = 0, total = 0;
static void check (bool ok, const char* name, const String& info)
{
    ++total; if (! ok) ++failures;
    std::printf ("[%s] %-56s %s\n", ok ? " OK " : "FAIL", name, info.toRawUTF8());
}
static float pv (KingLionProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
static void setP (KingLionProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void mouse (Component& c, const char* type, Point<float> pos, int clicks = 1, Point<float> downPos = {})
{
    auto src = Desktop::getInstance().getMainMouseSource();
    const auto now = Time::getCurrentTime();
    MouseEvent e (src, pos, ModifierKeys (ModifierKeys::leftButtonModifier), 1.0f, 0, 0, 0, 0, &c, &c, now,
                  downPos.isOrigin() ? pos : downPos, now, clicks, String (type) == "drag");
    const String t (type);
    if (t == "down") c.mouseDown (e);
    else if (t == "drag") c.mouseDrag (e);
    else if (t == "up") c.mouseUp (e);
    else if (t == "dbl") c.mouseDoubleClick (e);
}

static void sine (AudioBuffer<float>& b, double sr, double f, float amp, int start = 0)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (c, i, amp * (float) std::sin (2 * MathConstants<double>::pi * f * (start + i) / sr));
}
static void render (KingLionProcessor& p, AudioBuffer<float>& out, double sr, int blocks, int bs = 512, double f = 1000, float amp = 0.5f)
{
    out.setSize (p.getTotalNumOutputChannels(), blocks * bs);
    MidiBuffer m;
    for (int k = 0; k < blocks; ++k)
    {
        AudioBuffer<float> b (p.getTotalNumOutputChannels(), bs);
        sine (b, sr, f, amp, k * bs);
        p.processBlock (b, m);
        for (int c = 0; c < b.getNumChannels(); ++c) out.copyFrom (c, k * bs, b, c, 0, bs);
    }
}
static double rmsDb (const AudioBuffer<float>& b, int ch, int a, int n)
{
    return Decibels::gainToDecibels ((double) b.getRMSLevel (ch, a, n), -200.0);
}
static void savePng (Component& c, const File& f, float scale = 1.0f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    FileOutputStream os (f);
    PNGImageFormat().writeImageToStream (img, os);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File shots = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory().getChildFile ("Screens");
    const double sr = 48000;

    // ---------- áudio ----------
    {
        KingLionProcessor p; p.prepareToPlay (sr, 512);
        AudioBuffer<float> out; render (p, out, sr, 40);
        bool finite = true;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (c, i));
        const double lvl = rmsDb (out, 0, 10000, 10000);
        check (finite && std::abs (lvl - (-9.03)) < 0.2, "Padrão: passa o som (nível igual, cor leve)", String (lvl, 2) + " dB RMS (entrada −9,03)");
        check (p.getName() == "TUBEVOX" && p.getLatencySamples() == 0, "Nome e latência zero", p.getName() + ", " + String (p.getLatencySamples()) + " amostras");
    }

    // ---------- tela + mouse ----------
    {
        KingLionProcessor p; p.prepareToPlay (sr, 512);
        std::unique_ptr<KingLionEditor> ed (dynamic_cast<KingLionEditor*> (p.createEditor()));
        auto& panel = ed->getPanel();
        check (ed->getWidth() == KingLionPanel::W && ed->getHeight() == KingLionPanel::H, "Tela abre em 1180×740",
               String (ed->getWidth()) + "×" + String (ed->getHeight()));

        // THRESHOLD: arrastar para cima
        auto* thrKnob = dynamic_cast<klb::ui::Knob*> (panel.findControl (klb::ids::thr));
        auto& sl = thrKnob->getSlider();
        sl.setPopupDisplayEnabled (false, false, nullptr);   // o balão abre janela própria (não existe no Xvfb sem gerenciador)
        const auto c0 = sl.getLocalBounds().toFloat().getCentre();
        mouse (sl, "down", c0);
        for (int i = 1; i <= 10; ++i) mouse (sl, "drag", c0.translated (0, -10.0f * (float) i), 1, c0);
        mouse (sl, "up", c0.translated (0, -100), 1, c0);
        const float thr = pv (p, klb::ids::thr);
        check (thr > 2.0f, "Mouse: arrastar THRESHOLD para cima", "valor " + String (thr, 2));
        mouse (sl, "dbl", c0, 2);
        check (pv (p, klb::ids::thr) == 0.0f, "Mouse: duplo clique volta ao padrão", "valor " + String (pv (p, klb::ids::thr), 2));

        // GAIN (degraus): arrastar sobe um passo de cada vez
        auto& gs = dynamic_cast<klb::ui::Knob*> (panel.findControl (klb::ids::gain))->getSlider();
        gs.setPopupDisplayEnabled (false, false, nullptr);
        const auto g0 = gs.getLocalBounds().toFloat().getCentre();
        mouse (gs, "down", g0); mouse (gs, "drag", g0.translated (0, -60), 1, g0); mouse (gs, "up", g0.translated (0, -60), 1, g0);
        const float gainIdx = pv (p, klb::ids::gain);
        check (gainIdx > 2.5f && std::abs (gainIdx - std::round (gainIdx)) < 1e-6f, "Mouse: GANHO em degraus (40…60)",
               String::fromUTF8 ("índice ") + String (gainIdx, 0) + " = " + String (klb::kGainSteps[(size_t) gainIdx], 0) + " dB");

        // chaves
        auto* comp = panel.findControl (klb::ids::comp);
        mouse (*comp, "down", { 10, 10 });
        check (pv (p, klb::ids::comp) == 0.0f, "Chave COMPRIME 3:1 → BYPASS", "comp = " + String (pv (p, klb::ids::comp), 0));
        auto* hpf = panel.findControl (klb::ids::hpf);
        mouse (*hpf, "down", { 10, (float) hpf->getHeight() - 5 });   // já está em FLAT (embaixo): não muda
        mouse (*hpf, "down", { 10, 22 });                              // clique em cima: sobe para 80 Hz
        check (pv (p, klb::ids::hpf) == 1.0f, "Chave de 3 posições: FLAT → 80 Hz", "hpf = " + String (pv (p, klb::ids::hpf), 0));

        // preset pela lista
        panel.presets.setSelectedId (2, sendNotificationSync);
        check (pv (p, klb::ids::hpf) == 1.0f && std::abs (pv (p, klb::ids::thr) - 4.5f) < 1e-4f && pv (p, klb::ids::dsMode) == 1.0f
               && pv (p, klb::ids::comp) == 1.0f && pv (p, klb::ids::gain) == 2.0f,
               "Preset \"Voz Gospel Lead\" pela lista", "hpf 80, thr 4,5, de-esser, comp ligado, ganho 50");
        p.setCurrentProgram (0);
        bool allDefault = true;
        for (auto& id : klb::allParamIds())
            if (id != klb::ids::meter) { auto* q = p.apvts.getParameter (id); allDefault &= q->getValue() == q->getDefaultValue(); }
        check (allDefault, "Preset \"Inicial\" volta tudo ao padrão", allDefault ? "22/22" : "diferente");

        // medidor e lâmpada
        setP (p, klb::ids::thr, 6); setP (p, klb::ids::meter, 0);
        AudioBuffer<float> out; render (p, out, sr, 120, 512, 1000, 0.7f);
        for (int i = 0; i < 40; ++i) panel.updateMeter (0.033f);
        check (p.getMeterVu (0) < -5.0f, "VU em G-R mostra a redução", String (p.getMeterVu (0), 1) + " VU");
        setP (p, klb::ids::meter, 2); setP (p, klb::ids::thr, 0);    // PRÉ vem depois do compressor: medir sem compressão
        render (p, out, sr, 500, 512, 1000, 0.7f);                    // espera a soltura óptica lenta
        check (std::abs (p.getMeterVu (2) - (Decibels::gainToDecibels (0.7f) + 18.0f)) < 1.0f, "VU em PRÉ: 0 VU = −18 dBFS",
               String (p.getMeterVu (2), 1) + " VU para seno de " + String (Decibels::gainToDecibels (0.7f), 1) + " dBFS");

        savePng (*ed, shots.getChildFile ("tubevox.png"));
        {
            // tudo mexido: confere knobs girando e alavancas espelhando sobre a arte
            MemoryBlock keep; p.getStateInformation (keep);
            setP (p, klb::ids::input, 6); setP (p, klb::ids::gain, 4); setP (p, klb::ids::thr, 8); setP (p, klb::ids::attack, 0);
            setP (p, klb::ids::release, 4); setP (p, klb::ids::meter, 0); setP (p, klb::ids::lowF, 3); setP (p, klb::ids::midF, 0);
            setP (p, klb::ids::highF, 3); setP (p, klb::ids::lowG, -10); setP (p, klb::ids::midG, 5); setP (p, klb::ids::highG, 10);
            setP (p, klb::ids::dsFreq, 3); setP (p, klb::ids::dsThr, 10); setP (p, klb::ids::output, -24);
            setP (p, klb::ids::phase, 1); setP (p, klb::ids::comp, 0); setP (p, klb::ids::link, 0); setP (p, klb::ids::eq, 0);
            setP (p, klb::ids::power, 0); setP (p, klb::ids::hpf, 0); setP (p, klb::ids::dsListen, 0); setP (p, klb::ids::dsMode, 2);
            for (int i = 0; i < 3; ++i) panel.updateMeter (0.033f);
            savePng (*ed, shots.getChildFile ("tubevox-mexido.png"));
            setP (p, klb::ids::hpf, 1); setP (p, klb::ids::dsListen, 1); setP (p, klb::ids::dsMode, 1);
            savePng (*ed, shots.getChildFile ("tubevox-mexido2.png"));
            p.setStateInformation (keep.getData(), (int) keep.getSize());
        }
        ed->setSize ((int) (KingLionPanel::W * 1.5), (int) (KingLionPanel::H * 1.5));
        check (std::abs (panel.getTransform().mat00 - 1.5f) < 1e-3f, "Tela redimensiona em 150%", String (ed->getWidth()) + "×" + String (ed->getHeight()));
        savePng (*ed, shots.getChildFile ("tubevox-150.png"));
        auto* cons = ed->getConstrainer();
        Rectangle<int> want (0, 0, (int) (KingLionPanel::W * 0.5), (int) (KingLionPanel::H * 0.5));
        cons->setBoundsForComponent (ed.get(), want, false, false, true, true);
        check (ed->getWidth() >= (int) (KingLionPanel::W * 0.7) - 1 && cons->getMaximumWidth() == KingLionPanel::W * 2,
               "Redimensionar: limite de 70% a 200%", String::fromUTF8 ("pedido 590 px → ") + String (ed->getWidth()) + " px");
    }

    // ---------- estado salvo/reaberto ----------
    {
        KingLionProcessor a; a.prepareToPlay (sr, 512);
        setP (a, klb::ids::input, 2.3f); setP (a, klb::ids::gain, 3); setP (a, klb::ids::thr, 5.37f); setP (a, klb::ids::attack, 4);
        setP (a, klb::ids::release, 1); setP (a, klb::ids::link, 0); setP (a, klb::ids::midF, 6); setP (a, klb::ids::midG, -3.7f);
        setP (a, klb::ids::highG, 4.2f); setP (a, klb::ids::dsMode, 1); setP (a, klb::ids::dsThr, 4.44f); setP (a, klb::ids::output, -2.1f);
        setP (a, klb::ids::phase, 1); setP (a, klb::ids::hpf, 2); setP (a, klb::ids::meter, 3);
        MemoryBlock mb; a.getStateInformation (mb);
        KingLionProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize()); b.prepareToPlay (sr, 512);
        int same = 0;
        for (auto& id : klb::allParamIds())
            same += a.apvts.getParameter (id)->getValue() == b.apvts.getParameter (id)->getValue() ? 1 : 0;
        check (same == (int) klb::allParamIds().size(), "Projeto reaberto: todos os parâmetros exatos",
               String (same) + "/" + String ((int) klb::allParamIds().size()));
        a.prepareToPlay (sr, 512);
        AudioBuffer<float> oa, ob; render (a, oa, sr, 60); render (b, ob, sr, 60);
        bool ident = true;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < oa.getNumSamples(); ++i) ident &= oa.getSample (c, i) == ob.getSample (c, i);
        check (ident, "Projeto reaberto: áudio idêntico bit a bit", ident ? "0 amostras diferentes" : "diferente");
    }

    // ---------- desligado = original ----------
    {
        KingLionProcessor p; p.prepareToPlay (sr, 512);
        setP (p, klb::ids::thr, 8); setP (p, klb::ids::gain, 4); setP (p, klb::ids::power, 0);
        AudioBuffer<float> out; render (p, out, sr, 20);
        AudioBuffer<float> ref (2, out.getNumSamples()); sine (ref, sr, 1000, 0.5f);
        bool ident = true;
        for (int c = 0; c < 2; ++c) for (int i = 4800; i < out.getNumSamples(); ++i) ident &= out.getSample (c, i) == ref.getSample (c, i);
        check (ident, "LIGA/DESL. em DESL.: saída = entrada bit a bit", ident ? String::fromUTF8 ("idêntico") : "diferente");
    }

    // ---------- mono e 44,1 kHz ----------
    {
        KingLionProcessor p;
        AudioProcessor::BusesLayout mono; mono.inputBuses.add (AudioChannelSet::mono()); mono.outputBuses.add (AudioChannelSet::mono());
        const bool ok = p.setBusesLayout (mono);
        p.prepareToPlay (44100, 256);
        setP (p, klb::ids::thr, 5);
        AudioBuffer<float> out; render (p, out, 44100, 400, 256, 1000, 0.3f);
        const double gr = Decibels::gainToDecibels (0.3 / std::sqrt (2.0)) - rmsDb (out, 0, out.getNumSamples() - 8000, 8000);
        check (ok && out.getNumChannels() == 1 && gr > 5.0 && gr < 8.5, "Mono a 44,1 kHz: compressor age",
               String::fromUTF8 ("redução ~") + String (gr, 1) + " dB (−10,5 dBFS de pico, limiar −20)");
        check (! p.setBusesLayout ({ { AudioChannelSet::stereo() }, { AudioChannelSet::create5point1() } }), "Recusa layouts não suportados", "5.1 recusado");
    }

    std::printf ("\n%d/%d verificações OK\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
