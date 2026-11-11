// KingLion Box — testes do motor. Saída: uma linha por verificação; código 0 se tudo OK.
#include "../Source/Engine/KingLionEngine.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <chrono>
#include <string>
#include <functional>
#include <random>
#include <memory>

using namespace klb;

static int failures = 0, total = 0;
static void check (bool ok, const char* name, const std::string& info)
{
    ++total;
    if (! ok) ++failures;
    std::printf ("[%s] %-58s %s\n", ok ? " OK " : "FAIL", name, info.c_str());
}
static std::string fmt (const char* f, double a, double b = 0, double c = 0)
{
    char buf[256]; std::snprintf (buf, sizeof buf, f, a, b, c); return buf;
}

struct Buf
{
    std::vector<float> L, R;
    explicit Buf (size_t n) : L (n, 0.f), R (n, 0.f) {}
    size_t size() const { return L.size(); }
};

static void sine (std::vector<float>& v, double sr, double f, double ampDb, size_t a = 0, size_t b = SIZE_MAX)
{
    const double A = dbToGain (ampDb);
    b = std::min (b, v.size());
    for (size_t i = a; i < b; ++i) v[i] = (float) (A * std::sin (2 * kPi * f * (double) i / sr));
}

static void run (Engine& e, Buf& b, int block = 256)
{
    for (size_t i = 0; i < b.size(); i += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, b.size() - i);
        float* d[2] = { b.L.data() + i, b.R.data() + i };
        e.process (d, 2, n);
    }
}
static void runMono (Engine& e, std::vector<float>& v, int block = 256)
{
    for (size_t i = 0; i < v.size(); i += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, v.size() - i);
        float* d[1] = { v.data() + i };
        e.process (d, 1, n);
    }
}

static double rmsDb (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return 10 * std::log10 (std::max (s / (double) (b - a), 1e-30));
}
// Amplitude (dB de pico) de uma frequência por correlação (Goertzel simplificado).
static double toneDb (const std::vector<float>& v, size_t a, size_t b, double sr, double f)
{
    double re = 0, im = 0;
    for (size_t i = a; i < b; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * kPi * (double) (i - a) / (double) (b - a)); // Hann
        re += v[i] * w * std::cos (2 * kPi * f * (double) i / sr);
        im += v[i] * w * std::sin (2 * kPi * f * (double) i / sr);
    }
    const double amp = 2 * std::sqrt (re * re + im * im) / ((double) (b - a) * 0.5);
    return gainToDb (amp);
}

using EnginePtr = std::unique_ptr<Engine>;
static EnginePtr make (double sr, const Params& p, int ch = 2)
{
    auto e = std::make_unique<Engine>(); e->setParams (p); e->prepare (sr, ch); return e;
}
static void run (EnginePtr& e, Buf& b, int block = 256) { run (*e, b, block); }
static void runMono (EnginePtr& e, std::vector<float>& v, int block = 256) { runMono (*e, v, block); }
// Ganho de pequeno sinal numa frequência (dB), já descontando a cadeia neutra.
static double gainAt (double sr, const Params& p, double f, double levelDb = -40)
{
    auto e = make (sr, p);
    Buf b ((size_t) (sr * 1.0));
    sine (b.L, sr, f, levelDb); b.R = b.L;
    run (e, b);
    return toneDb (b.L, (size_t) (sr * 0.5), b.size(), sr, f) - levelDb;
}

int main()
{
    const double sr = 48000;
    Params neutral;  // padrão: EQ plano, compressor desligado, pré em 50 dB

    // ---------- pré valvulado ----------
    {
        const double g50 = gainAt (sr, neutral, 1000);
        check (std::fabs (g50) < 0.1, "Pré 50 dB: ganho unitário em sinal baixo", fmt ("%.3f dB", g50));
        Params p = neutral; p.gainStep = 4;
        const double g60 = gainAt (sr, p, 1000);
        check (std::fabs (g60 - 10.0) < 0.15, "Pré 60 dB: +10 dB", fmt ("%.2f dB", g60));
        p.gainStep = 0;
        const double g40 = gainAt (sr, p, 1000);
        check (std::fabs (g40 + 10.0) < 0.15, "Pré 40 dB: −10 dB", fmt ("%.2f dB", g40));

        // resposta plana na voz (100 Hz – 10 kHz) com tudo neutro
        double worst = 0;
        for (double f : { 100.0, 300.0, 1000.0, 3000.0, 10000.0 })
            worst = std::max (worst, std::fabs (gainAt (sr, neutral, f)));
        check (worst < 0.35, "Resposta plana 100 Hz–10 kHz (neutro)", fmt ("desvio máx %.2f dB", worst));
        const double g30 = gainAt (sr, neutral, 30);
        check (g30 > 0.1 && g30 < 1.0, "Transformador: leve corpo em 30 Hz", fmt ("%+.2f dB", g30));
    }
    // harmônicos: limpo em nível baixo, 2º dominante quando empurrado
    {
        auto harm = [&] (int gainStep, double lvl, double& h2, double& h3)
        {
            Params p = neutral; p.gainStep = gainStep;
            auto e = make (sr, p);
            Buf b ((size_t) sr); sine (b.L, sr, 500, lvl); b.R = b.L; run (e, b);
            const size_t a = (size_t) (sr * 0.5);
            const double f1 = toneDb (b.L, a, b.size(), sr, 500);
            h2 = toneDb (b.L, a, b.size(), sr, 1000) - f1;
            h3 = toneDb (b.L, a, b.size(), sr, 1500) - f1;
        };
        double h2, h3;
        harm (2, -30, h2, h3);
        check (h2 < -60 && h3 < -60, "−30 dBFS no pré 50: limpo", fmt ("H2 %.1f dB, H3 %.1f dB", h2, h3));
        harm (2, -6, h2, h3);
        check (h2 < -25 && h2 > -60, "−6 dBFS no pré 50: cor sutil", fmt ("H2 %.1f dB, H3 %.1f dB", h2, h3));
        harm (4, -6, h2, h3);
        check (h2 > -45 && h2 > h3, "Pré 60 empurrado: 2º harmônico domina (válvula)", fmt ("H2 %.1f dB, H3 %.1f dB", h2, h3));
    }
    // ---------- corte de graves e fase ----------
    {
        Params p = neutral; p.hpf = 1;
        const double a80 = gainAt (sr, p, 80) - gainAt (sr, neutral, 80);
        const double a40 = gainAt (sr, p, 40) - gainAt (sr, neutral, 40);
        check (std::fabs (a80 + 3.0) < 0.3, "Corte 80 Hz: −3 dB em 80 Hz", fmt ("%.2f dB", a80));
        check (std::fabs (a40 + 12.3) < 0.8, "Corte 80 Hz: 12 dB/oitava (40 Hz)", fmt ("%.2f dB", a40));
        p.hpf = 2;
        const double a120 = gainAt (sr, p, 120) - gainAt (sr, neutral, 120);
        check (std::fabs (a120 + 3.0) < 0.3, "Corte 120 Hz: −3 dB em 120 Hz", fmt ("%.2f dB", a120));

        Params q = neutral; q.phase180 = true;
        auto e1 = make (sr, neutral), e2 = make (sr, q);
        Buf b1 ((size_t) sr / 2), b2 ((size_t) sr / 2);
        sine (b1.L, sr, 440, -30); b1.R = b1.L; b2 = b1;
        run (e1, b1); run (e2, b2);
        std::vector<float> sum (b1.size());
        for (size_t i = 0; i < b1.size(); ++i) sum[i] = b1.L[i] + b2.L[i];
        const double rel = rmsDb (sum, 4800, sum.size()) - rmsDb (b1.L, 4800, b1.size());
        check (rel < -60, "Fase 180°: polaridade invertida", fmt ("soma normal+invertido %.1f dB", rel));
    }
    // ---------- compressor óptico ----------
    {
        auto grFor = [&] (double lvl, float thrKnob) -> double
        {
            Params p = neutral; p.compOn = true; p.compThr = thrKnob;
            auto on = make (sr, p), off = make (sr, neutral);
            Buf a ((size_t) (sr * 3)), b ((size_t) (sr * 3));
            sine (a.L, sr, 1000, lvl); a.R = a.L; b = a;
            run (on, a); run (off, b);
            const size_t s = (size_t) (sr * 2.5);
            return rmsDb (b.L, s, a.size()) - rmsDb (a.L, s, a.size());
        };
        // limiar −20 dBFS (knob 5); seno a −10 → 10 dB acima → 3:1 = 6,67 dB
        const double g10 = grFor (-10, 5);
        check (std::fabs (g10 - 6.67) < 0.4, "3:1 — 10 dB acima do limiar", fmt ("redução %.2f dB (esperado 6,67)", g10));
        const double g20 = grFor (0, 5);
        check (std::fabs (g20 - 13.33) < 0.8, "3:1 — 20 dB acima do limiar", fmt ("redução %.2f dB (esperado 13,3)", g20));
        const double g0 = grFor (-35, 5);
        check (g0 < 0.05, "Abaixo do limiar: sem compressão", fmt ("redução %.3f dB", g0));
        const double gmax = grFor (0, 10);
        check (gmax > 14 && gmax < 18.5, "Redução máxima satura (~16–18 dB)", fmt ("%.2f dB com 40 dB acima", gmax));

        // ataque: tempo até 63% da redução final, por posição
        double prevT = 0; bool mono = true; std::string info;
        for (int a = 0; a < 5; ++a)
        {
            Params p = neutral; p.compOn = true; p.compThr = 5; p.attack = a;
            auto e = make (sr, p);
            std::vector<float> v ((size_t) sr, 0.f);
            sine (v, sr, 1000, -40, 0, (size_t) (sr * 0.2));
            sine (v, sr, 1000, 0, (size_t) (sr * 0.2), v.size());
            // mede o ganho instantâneo pelo envelope (bloco de 1 ms)
            auto ref = make (sr, neutral);
            std::vector<float> r = v;
            runMono (e, v); runMono (ref, r);
            const size_t on = (size_t) (sr * 0.2), win = (size_t) (sr * 0.001);
            const double final = rmsDb (r, v.size() - 4800, v.size()) - rmsDb (v, v.size() - 4800, v.size());
            double t63 = -1;
            for (size_t k = on; k + win < v.size(); k += win / 2)
            {
                const double gr = rmsDb (r, k, k + win) - rmsDb (v, k, k + win);
                if (gr >= 0.63 * final) { t63 = (double) (k + win / 2 - on) / sr * 1000; break; }
            }
            info += fmt ("%.0f ", t63);
            if (t63 <= prevT) mono = false;
            prevT = t63;
            if (a == 0) check (t63 > 2 && t63 < 12, "Ataque FAST (~4 ms + detector)", fmt ("%.1f ms", t63));
            if (a == 4) check (t63 > 50 && t63 < 110, "Ataque SLOW (~70 ms)", fmt ("%.1f ms", t63));
        }
        check (mono, "Ataque: 5 posições em ordem crescente", info + "ms");

        // soltura dependente do programa: compressão longa solta mais devagar
        auto releaseTime = [&] (double burstSec) -> double
        {
            Params p = neutral; p.compOn = true; p.compThr = 6; p.release = 2;
            auto e = make (sr, p), ref = make (sr, neutral);
            const size_t n0 = (size_t) (sr * burstSec), n = n0 + (size_t) (sr * 4);
            std::vector<float> v (n, 0.f);
            sine (v, sr, 1000, -3, 0, n0);
            sine (v, sr, 1000, -40, n0, n);
            std::vector<float> r = v;
            runMono (e, v); runMono (ref, r);
            const size_t win = (size_t) (sr * 0.005);
            const double start = rmsDb (r, n0 - win * 4, n0) - rmsDb (v, n0 - win * 4, n0);
            for (size_t k = n0; k + win < n; k += win)
            {
                const double gr = rmsDb (r, k, k + win) - rmsDb (v, k, k + win);
                if (gr < start * 0.1) return (double) (k - n0) / sr;
            }
            return 99;
        };
        const double rShort = releaseTime (0.15), rLong = releaseTime (3.0);
        check (rLong > rShort * 1.5, "Soltura óptica: compressão longa solta mais devagar",
               fmt ("pico curto %.2f s × longo %.2f s (até 90%%)", rShort, rLong));

        // link estéreo
        Params p = neutral; p.compOn = true; p.compThr = 5; p.link = true;
        auto e = make (sr, p);
        Buf b ((size_t) (sr * 2)); sine (b.L, sr, 1000, 0); sine (b.R, sr, 1000, -40);
        run (e, b);
        auto ref = make (sr, neutral);
        Buf r ((size_t) (sr * 2)); sine (r.L, sr, 1000, 0); sine (r.R, sr, 1000, -40); run (ref, r);
        const size_t s = (size_t) (sr * 1.5);
        const double grL = rmsDb (r.L, s, b.size()) - rmsDb (b.L, s, b.size());
        const double grR = rmsDb (r.R, s, b.size()) - rmsDb (b.R, s, b.size());
        check (std::fabs (grL - grR) < 0.6 && grR > 10, "LINK: o lado baixo comprime junto", fmt ("L %.2f dB, R %.2f dB", grL, grR));
        p.link = false; auto e2 = make (sr, p);
        Buf b2 ((size_t) (sr * 2)); sine (b2.L, sr, 1000, 0); sine (b2.R, sr, 1000, -40); run (e2, b2);
        const double grR2 = rmsDb (r.R, s, b2.size()) - rmsDb (b2.R, s, b2.size());
        check (grR2 < 0.05, "SEPARADO: cada lado comprime sozinho", fmt ("R %.3f dB", grR2));
    }
    // ---------- EQ ----------
    {
        Params p = neutral; p.midFreq = 3; p.midGain = 10;     // 800 Hz +10
        const double m = gainAt (sr, p, 800) - gainAt (sr, neutral, 800);
        check (std::fabs (m - 10) < 0.2, "MÉDIO +10 dB em 800 Hz", fmt ("%.2f dB", m));
        p.midGain = -10;
        const double md = gainAt (sr, p, 800) - gainAt (sr, neutral, 800);
        check (std::fabs (md + 10) < 0.2, "MÉDIO −10 dB (vale) em 800 Hz", fmt ("%.2f dB", md));
        Params q = neutral; q.lowFreq = 2; q.lowGain = 6;      // 110 Hz
        const double l = gainAt (sr, q, 110) - gainAt (sr, neutral, 110);
        const double l2k = gainAt (sr, q, 2000) - gainAt (sr, neutral, 2000);
        check (std::fabs (l - 6) < 0.2 && std::fabs (l2k) < 0.3, "GRAVE +6 em 110 Hz, 2 kHz intacto", fmt ("%.2f / %.2f dB", l, l2k));
        Params h = neutral; h.highFreq = 2; h.highGain = 8;    // 12 kHz
        const double hh = gainAt (sr, h, 12000) - gainAt (sr, neutral, 12000);
        check (std::fabs (hh - 8) < 0.4, "AGUDO +8 em 12 kHz (ar)", fmt ("%.2f dB", hh));
        Params off = p; off.eqOn = false;
        const double o = gainAt (sr, off, 800) - gainAt (sr, neutral, 800);
        check (std::fabs (o) < 0.01, "EQ desligado ignora os ganhos", fmt ("%.3f dB", o));
    }
    // ---------- de-esser e limitador ----------
    {
        auto twoTone = [&] (const Params& p, double& dS, double& dV)
        {
            auto e = make (sr, p), ref = make (sr, neutral);
            Buf a ((size_t) (sr * 1.5));
            for (size_t i = 0; i < a.size(); ++i)
                a.L[i] = (float) (0.1 * std::sin (2 * kPi * 300 * i / sr) + 0.3 * std::sin (2 * kPi * 6000 * i / sr));
            a.R = a.L; Buf b = a;
            run (e, a); run (ref, b);
            const size_t s = (size_t) (sr * 0.8);
            dS = toneDb (a.L, s, a.size(), sr, 6000) - toneDb (b.L, s, b.size(), sr, 6000);
            dV = toneDb (a.L, s, a.size(), sr, 300) - toneDb (b.L, s, b.size(), sr, 300);
        };
        Params p = neutral; p.dsMode = 1; p.dsFreq = 1; p.dsThr = 6;
        double dS, dV; twoTone (p, dS, dV);
        check (dS < -6 && std::fabs (dV) < 0.3, "DE-ESSER 6 kHz: corta o S, vogal intacta", fmt ("6 kHz %.1f dB, 300 Hz %.2f dB", dS, dV));
        p.dsThr = 0; twoTone (p, dS, dV);
        check (std::fabs (dS) < 0.1, "DE-ESSER com limiar acima do sinal: não age", fmt ("%.2f dB", dS));

        Params l = neutral; l.dsMode = 2; l.dsThr = 3;   // −12 dBFS
        auto e = make (sr, l), ref = make (sr, neutral);
        Buf a ((size_t) (sr * 2)); sine (a.L, sr, 1000, 0); a.R = a.L; Buf b = a;
        run (e, a); run (ref, b);
        const size_t s = (size_t) (sr * 1.5);
        double pk = 0; for (size_t i = s; i < a.size(); ++i) pk = std::max (pk, (double) std::fabs (a.L[i]));
        double pr = 0; for (size_t i = s; i < b.size(); ++i) pr = std::max (pr, (double) std::fabs (b.L[i]));
        const double over = gainToDb (pr) + 12, outOver = gainToDb (pk) + 12;
        check (outOver < over / 10 + 1.0, "LIMITADOR 10:1", fmt ("entrada %.1f dB acima → saída %.2f dB acima", over, outOver));

        Params li = neutral; li.dsMode = 1; li.dsListen = true;
        auto el = make (sr, li);
        Buf c ((size_t) sr); sine (c.L, sr, 300, -10); c.R = c.L; run (el, c);
        const double lv = rmsDb (c.L, (size_t) (sr * 0.5), c.size());
        check (lv < -35, "OUVIR (side chain): só a banda do S", fmt ("300 Hz sai em %.1f dBFS", lv));
    }
    // ---------- saída, liga/desliga, robustez ----------
    {
        Params p = neutral; p.outputDb = -6;
        const double g = gainAt (sr, p, 1000) - gainAt (sr, neutral, 1000);
        check (std::fabs (g + 6) < 0.02, "SAÍDA −6 dB", fmt ("%.3f dB", g));

        Params off = neutral; off.power = false; off.compOn = true; off.compThr = 8; off.gainStep = 4;
        auto e = make (sr, off);
        Buf b ((size_t) sr); std::mt19937 rng (1); std::normal_distribution<float> nd (0, 0.2f);
        for (size_t i = 0; i < b.size(); ++i) { b.L[i] = nd (rng); b.R[i] = nd (rng); }
        Buf orig = b; run (e, b);
        bool same = b.L == orig.L && b.R == orig.R;
        check (same, "DESLIGADO: saída idêntica bit a bit", same ? "0 amostras diferentes" : "diferente");

        // ligar no meio sem clique
        Params on = neutral; on.power = false;
        auto e2 = make (sr, on);
        std::vector<float> v ((size_t) sr); sine (v, sr, 200, -6);
        double maxStep = 0;
        for (size_t i = 0; i < v.size(); i += 256)
        {
            if (i == 24000) { on.power = true; on.gainStep = 3; e2->setParams (on); }
            float* d[1] = { v.data() + i }; e2->process (d, 1, (int) std::min<size_t> (256, v.size() - i));
        }
        for (size_t i = 1; i < v.size(); ++i) maxStep = std::max (maxStep, (double) std::fabs (v[i] - v[i - 1]));
        const double sineStep = dbToGain (-6 + 5) * 2 * kPi * 200 / sr;
        check (maxStep < sineStep * 1.3, "Ligar + mudar o ganho no meio: sem clique", fmt ("passo máx %.4f (seno %.4f)", maxStep, sineStep));

        // silêncio → silêncio exato, sem DC
        Params all = neutral; all.compOn = true; all.compThr = 5; all.dsMode = 1; all.lowGain = 5;
        auto es = make (sr, all);
        Buf z ((size_t) sr); run (es, z);
        double mz = 0; for (float x : z.L) mz = std::max (mz, (double) std::fabs (x));
        check (mz == 0.0, "Silêncio entra, silêncio sai (sem DC da válvula)", fmt ("máx %.1e", mz));

        // sinal extremo: sem NaN
        Params ext = neutral; ext.gainStep = 4; ext.inputDb = 12; ext.lowGain = 10; ext.highGain = 10; ext.midGain = 10;
        auto ee = make (sr, ext);
        Buf x ((size_t) sr); for (size_t i = 0; i < x.size(); ++i) x.L[i] = x.R[i] = (i % 97 < 48) ? 4.f : -4.f;
        run (ee, x);
        bool finite = true; double mx = 0;
        for (float s : x.L) { finite &= std::isfinite (s); mx = std::max (mx, (double) std::fabs (s)); }
        check (finite && mx < 40, "Sinal extremo (+12 dB, pré 60, EQ no máximo): estável", fmt ("pico %.1f", mx));
    }
    // ---------- taxas de amostragem e mono ----------
    {
        for (double r : { 44100.0, 96000.0 })
        {
            Params p = neutral; p.midFreq = 6; p.midGain = 6; p.hpf = 1;
            const double m = gainAt (r, p, 3500) - gainAt (r, neutral, 3500);
            const double h = gainAt (r, p, 80) - gainAt (r, neutral, 80);
            check (std::fabs (m - 6) < 0.25 && std::fabs (h + 3) < 0.35,
                   r == 44100 ? "44,1 kHz: EQ e corte corretos" : "96 kHz: EQ e corte corretos", fmt ("%.2f / %.2f dB", m, h));
        }
        Params p = neutral; p.compOn = true; p.compThr = 5;
        auto e = make (sr, p, 1);
        std::vector<float> v ((size_t) (sr * 3)); sine (v, sr, 1000, -10);
        std::vector<float> r = v; auto ref = make (sr, neutral, 1);
        runMono (e, v); runMono (ref, r);
        const double gr = rmsDb (r, (size_t) (sr * 2.5), v.size()) - rmsDb (v, (size_t) (sr * 2.5), v.size());
        check (std::fabs (gr - 6.67) < 0.4, "Mono: compressor igual ao estéreo", fmt ("%.2f dB", gr));
    }
    // ---------- custo ----------
    {
        Params p = neutral; p.compOn = true; p.compThr = 5; p.dsMode = 1; p.dsThr = 4; p.lowGain = 3; p.midGain = -2; p.highGain = 4;
        auto e = make (sr, p);
        Buf b ((size_t) (sr * 20)); std::mt19937 rng (2); std::normal_distribution<float> nd (0, 0.1f);
        for (size_t i = 0; i < b.size(); ++i) { b.L[i] = nd (rng); b.R[i] = nd (rng); }
        const auto t0 = std::chrono::steady_clock::now();
        run (e, b, 128);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        check (secs / 20.0 < 0.05, "CPU (estéreo 48 kHz, tudo ligado)", fmt ("%.2f%% de um núcleo", secs / 20.0 * 100));
    }

    std::printf ("\n%d/%d verificações OK\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
