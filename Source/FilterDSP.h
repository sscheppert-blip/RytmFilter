#pragma once

#include <cmath>
#include <complex>
#include <algorithm>
#include "Params.h"

namespace dsp
{
    constexpr double pi = 3.14159265358979323846;

    //==============================================================================
    // Koeffizienten fuer einen Filterzustand (pro Sample berechnet, fuer alle Kanaele gleich)
    struct SvfCoeffs
    {
        float a1 = 0, a2 = 0, a3 = 0, k = 2;
        float type = 0;
        float bpGain = 1, pkGain = 0; // Typ-spezifische Ausgangsmischung

        void set (double cutoffHz, double resValue, int filterType, double sampleRate)
        {
            const double fc = std::clamp (cutoffHz, 10.0, sampleRate * 0.45);
            const double g  = std::tan (pi * fc / sampleRate);
            const double r  = std::clamp (resValue, 0.0, 127.0) / 127.0;

            double kk;
            if (filterType == Params::PK)
            {
                // Peak/Glocke: RES steuert Anhebung (0..+24 dB) und Schmalheit
                const double gainDb = 24.0 * r;
                const double A      = std::pow (10.0, gainDb / 40.0);
                const double q      = 0.6 + 3.0 * r;
                kk     = 1.0 / (q * A);
                pkGain = (float) (A * A - 1.0);
            }
            else
            {
                kk = 1.0 / Params::resToQ (resValue);
                pkGain = 0.0f;
            }

            k  = (float) kk;
            const double d = 1.0 / (1.0 + g * (g + kk));
            a1 = (float) d;
            a2 = (float) (g * d);
            a3 = (float) (g * g * d);
            type = (float) filterType;
            bpGain = (float) (kk * (1.0 + 3.0 * r)); // BP: Spitzenpegel 0 dB (RES 0) .. +12 dB (RES 127)
        }
    };

    //==============================================================================
    // Zustandsvariablen-Filter (Trapez-integriert, Cytomic-Topologie) mit weicher Saettigung
    // der Integratoren -> analoges Verhalten bei hoher Resonanz, kein Aufschwingen ins Unendliche.
    struct MultimodeSvf
    {
        float ic1 = 0, ic2 = 0;

        void reset() { ic1 = ic2 = 0; }

        // Exakt linear bis |x| = 2 (keine Daempfung bei normalen Pegeln), darueber weiches Knie
        // mit Grenze 4 -> analoges "Einbremsen" bei extremer Resonanz statt Explodieren.
        static float sat (float x)
        {
            constexpr float t = 2.0f, range = 2.0f;
            const float a = std::abs (x);
            if (a <= t)
                return x;
            const float over = a - t;
            return std::copysign (t + over / (1.0f + over / range), x);
        }

        float process (float x, const SvfCoeffs& c)
        {
            const float v3 = x - ic2;
            const float v1 = c.a1 * ic1 + c.a2 * v3;
            const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
            ic1 = sat (2.0f * v1 - ic1);
            ic2 = sat (2.0f * v2 - ic2);

            const float lp = v2, bp = v1, hp = x - c.k * v1 - v2;

            switch ((int) c.type)
            {
                case Params::LP2: return lp;
                case Params::LP1: return lp + bp;              // bei RES 0 exakt 1-polig (6 dB/Okt)
                case Params::BP:  return bp * c.bpGain;
                case Params::HP1: return hp + bp;              // bei RES 0 exakt 1-polig
                case Params::HP2: return hp;
                case Params::BS:  return x - c.k * bp;         // Notch, bei RES 0 am breitesten
                case Params::PK:  return x + c.pkGain * c.k * bp;
                default:          return lp;
            }
        }
    };

    // Analytischer Betragsfrequenzgang (fuer Display und Tests), f relativ zur Grenzfrequenz
    inline double magnitude (double f, double cutoffHz, double resValue, int type)
    {
        SvfCoeffs c;
        c.set (cutoffHz, resValue, type, 1.0e9); // praktisch analoger Prototyp
        const std::complex<double> s (0.0, f / cutoffHz);
        const double k = c.k;
        const std::complex<double> den = s * s + k * s + 1.0;
        const std::complex<double> lp = 1.0 / den, bp = s / den, hp = s * s / den;

        std::complex<double> h;
        switch (type)
        {
            case Params::LP2: h = lp; break;
            case Params::LP1: h = lp + bp; break;
            case Params::BP:  h = bp * (double) c.bpGain; break;
            case Params::HP1: h = hp + bp; break;
            case Params::HP2: h = hp; break;
            case Params::BS:  h = 1.0 - k * bp; break;
            case Params::PK:  h = 1.0 + (double) c.pkGain * k * bp; break;
            default:          h = lp; break;
        }
        return std::abs (h);
    }

    //==============================================================================
    // ADSR-Huellkurve: linearer Attack, exponentielles Decay/Release, Retrigger ab aktuellem Pegel
    struct Envelope
    {
        enum Stage { idle, attack, decay, sustain, release };

        Stage stage = idle;
        double level = 0.0;
        double sampleRate = 44100.0;

        void reset() { stage = idle; level = 0.0; }

        void gateOn()  { stage = attack; }
        void gateOff() { if (stage != idle) stage = release; }

        double process (double atkV, double decV, double susV, double relV)
        {
            const double sus = std::clamp (susV, 0.0, 127.0) / 127.0;

            switch (stage)
            {
                case idle: level = 0.0; break;

                case attack:
                {
                    const double ms = Params::envTimeMs (atkV);
                    level += ms <= 0.0 ? 1.0 : 1000.0 / (ms * sampleRate);
                    if (level >= 1.0) { level = 1.0; stage = decay; }
                    break;
                }

                case decay:
                    level = sus + (level - sus) * coef (decV);
                    if (std::abs (level - sus) < 1.0e-4) { level = sus; stage = sustain; }
                    break;

                case sustain:
                    level = sus;
                    break;

                case release:
                    level *= coef (relV);
                    if (level < 1.0e-4) { level = 0.0; stage = idle; }
                    break;
            }
            return level;
        }

    private:
        // Zeit = Dauer bis -60 dB der Strecke
        double coef (double v) const
        {
            const double ms = Params::envTimeMs (v);
            if (ms <= 0.0) return 0.0;
            return std::exp (-6.907755 / (ms * 0.001 * sampleRate));
        }
    };

    //==============================================================================
    // Transienten-Erkennung: schnelle vs. langsame Huellkurve, mit Sperrzeit
    struct TransientDetector
    {
        double sampleRate = 44100.0;
        double fast = 0.0, slow = 0.0;
        int holdoff = 0;

        void reset() { fast = slow = 0.0; holdoff = 0; }

        // sens 0..127: hoeher = empfindlicher
        bool process (float x, double sens)
        {
            const double a  = std::abs ((double) x);
            const double s  = std::clamp (sens, 0.0, 127.0) / 127.0;

            const double fastAtk = std::exp (-1.0 / (0.0005 * sampleRate));
            const double fastRel = std::exp (-1.0 / (0.010 * sampleRate));
            const double slowRel = std::exp (-1.0 / (0.150 * sampleRate));

            fast = a > fast ? a + (fast - a) * fastAtk : a + (fast - a) * fastRel;
            slow = fast > slow ? fast + (slow - fast) * std::exp (-1.0 / (0.030 * sampleRate))
                               : fast + (slow - fast) * slowRel;

            if (holdoff > 0) { --holdoff; return false; }

            const double ratio = 4.0 - 2.7 * s;                          // 4.0 .. 1.3
            const double floor = std::pow (10.0, (-20.0 - 40.0 * s) / 20.0); // -20 .. -60 dBFS

            if (fast > floor && fast > slow * ratio + 1.0e-6)
            {
                holdoff = (int) (0.06 * sampleRate);
                return true;
            }
            return false;
        }
    };
}
