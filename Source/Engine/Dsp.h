// KingLion Box — blocos de DSP (WYATT Sounds). Sem dependência do JUCE.
#pragma once
#include <cmath>
#include <algorithm>

namespace klb
{
constexpr double kPi = 3.14159265358979323846;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g)  { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

// Biquad (forma direta transposta II), coeficientes RBJ.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void norm (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
    void setHighpass (double sr, double f, double q)
    {
        const double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setLowpass (double sr, double f, double q)
    {
        const double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    // Passa-banda com pico de 0 dB no centro.
    void setBandpass (double sr, double f, double q)
    {
        const double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        norm (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void setPeak (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void setLowShelf (double sr, double f, double gainDb, double slope = 1.0)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * kPi * f / sr, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2);
        const double sa = 2 * std::sqrt (A) * al;
        norm (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
              (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void setHighShelf (double sr, double f, double gainDb, double slope = 1.0)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * kPi * f / sr, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2);
        const double sa = 2 * std::sqrt (A) * al;
        norm (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
              (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

// Estágio valvulado: curva assimétrica tanh(k·x + b) − tanh(b), normalizada para ganho 1
// em sinal baixo. Assimetria (b ≠ 0) gera 2º harmônico, como um triodo; b = 0 é simétrico.
// O caminho linear passa direto (sem atraso nem perda de agudos); só a parte não linear
// (o "resíduo" = curva − reta tangente) recebe antialiasing por antiderivada (ADAA de 1ª ordem).
struct TubeStage
{
    double k = 0.5, b = 0.2;
    double tb = 0, s = 1, norm = 1;
    double prevU = 0, prevR = 0;

    void setup (double drive, double bias)
    {
        k = drive; b = bias;
        tb = std::tanh (b);
        const double sech = 1.0 / std::cosh (b);
        s = sech * sech;
        norm = 1.0 / (k * s);
        reset();
    }
    void reset() { prevU = 0; prevR = R (0); }

    static double logCosh (double v)
    {
        const double a = std::fabs (v);
        return a + std::log1p (std::exp (-2 * a)) - 0.6931471805599453;
    }
    // resíduo r(u) = tanh(u+b) − tanh(b) − s·u  e sua antiderivada R(u)
    double r (double u) const { return std::tanh (u + b) - tb - s * u; }
    double R (double u) const { return logCosh (u + b) - u * tb - 0.5 * s * u * u; }

    inline double process (double x)
    {
        const double u = k * x;
        const double Ru = R (u);
        const double du = u - prevU;
        double res;
        if (std::fabs (du) > 1.0e-5)
            res = (Ru - prevR) / du;
        else
            res = r (0.5 * (u + prevU));
        prevU = u; prevR = Ru;
        return x + res * norm;
    }
};

// Bloqueio de DC (passa-altas de 1 polo).
struct DcBlock
{
    double R = 0.9995, x1 = 0, y1 = 0;
    void setup (double sr, double f = 5.0) { R = std::exp (-2 * kPi * f / sr); reset(); }
    void reset() { x1 = y1 = 0; }
    inline double process (double x) { const double y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
};

struct OnePoleLP
{
    double a = 0, z = 0;
    void setup (double sr, double f) { a = std::exp (-2 * kPi * f / sr); z = 0; }
    inline double process (double x) { z = x + a * (z - x); return z; }
};

// Coeficiente de suavização exponencial para uma constante de tempo em ms.
inline double timeCoef (double sr, double ms) { return ms <= 0 ? 0.0 : std::exp (-1.0 / (sr * ms * 0.001)); }

// Célula óptica (estilo opto/LDR): ataque com constante própria e soltura em dois estágios.
// O estágio lento "carrega" enquanto a compressão dura, então compressões longas
// soltam mais devagar (dependência do programa, típica dos limitadores ópticos).
struct OptoCell
{
    double attC = 0, relFastC = 0, relSlowC = 0, chargeC = 0;
    double fast = 0, slow = 0;  // redução em dB (positiva)

    void setup (double sr, double attackMs, double releaseMs)
    {
        attC = timeCoef (sr, attackMs);
        relFastC = timeCoef (sr, releaseMs);
        relSlowC = timeCoef (sr, releaseMs * 6.0);
        chargeC  = timeCoef (sr, 1500.0);
    }
    void reset() { fast = slow = 0; }

    inline double process (double targetDb)
    {
        if (targetDb > fast) fast = targetDb + attC * (fast - targetDb);
        else                 fast = targetDb + relFastC * (fast - targetDb);

        // memória: segue a redução devagar enquanto há compressão; solta em 6× o release
        const double memTarget = 0.6 * fast;
        if (memTarget > slow) slow = memTarget + chargeC * (slow - memTarget);
        else                  slow = memTarget + relSlowC * (slow - memTarget);

        return std::max (fast, slow);
    }
};

// Curva estática com joelho suave. Retorna a redução em dB (positiva).
inline double kneeReduction (double overDb, double ratio, double kneeDb)
{
    const double s = 1.0 - 1.0 / ratio;
    if (overDb <= -kneeDb / 2) return 0.0;
    if (overDb >= kneeDb / 2)  return s * overDb;
    const double t = overDb + kneeDb / 2;
    return s * t * t / (2 * kneeDb);
}
} // namespace klb
