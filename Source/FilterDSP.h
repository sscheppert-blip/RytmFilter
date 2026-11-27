#pragma once

#include <cmath>
#include <complex>
#include <algorithm>
#include <cstdint>
#include <vector>
#include "Params.h"

namespace dsp
{
    constexpr double pi = 3.14159265358979323846;

    //==============================================================================
    // Koeffizienten fuer einen Filterzustand (pro Sample berechnet, fuer alle Kanaele gleich).
    // Die eigentliche Loesung (a1..a3) rechnet der Filter selbst, weil die Daempfung pegelabhaengig ist.
    struct SvfCoeffs
    {
        float g = 0.1f, k = 2, limit = 0;
        float type = 0;
        float bpGain = 1, pkGain = 0; // Typ-spezifische Ausgangsmischung

        void set (double cutoffHz, double resValue, int filterType, double sampleRate)
        {
            const double fc = std::clamp (cutoffHz, 10.0, sampleRate * 0.45);
            const double r  = std::clamp (resValue, 0.0, 127.0) / 127.0;

            double kk;
            if (filterType == Params::PK)
            {
                // Peak/Glocke: RES steuert Anhebung (0..+24 dB) und Schmalheit, keine Selbstoszillation
                const double gainDb = 24.0 * r;
                const double A      = std::pow (10.0, gainDb / 40.0);
                const double q      = 0.6 + 3.0 * r;
                kk     = 1.0 / (q * A);
                pkGain = (float) (A * A - 1.0);
                limit  = 0.0f;
            }
            else
            {
                kk     = Params::resToK (resValue); // ab RES 96 negativ -> Selbstoszillation
                pkGain = 0.0f;
                limit  = (float) Params::resLimit (resValue);
            }

            g    = (float) std::tan (pi * fc / sampleRate);
            k    = (float) kk;
            type = (float) filterType;
            // BP: Spitzenpegel 0 dB (RES 0) .. ca. +10 dB (RES 88); darueber nicht weiter angehoben
            bpGain = (float) (std::max (kk, 1.0 / 20.0) * (1.0 + 3.0 * r));
        }
    };

    //==============================================================================
    // Zustandsvariablen-Filter (Trapez-integriert, Cytomic-Topologie).
    // Analoges Verhalten bei hoher Resonanz: Die Daempfung steigt mit der Energie im Filter
    // (k_eff = k + limit * (bp^2 + lp^2)). Bei negativem k (RES > 96) schwingt der Filter
    // dadurch stabil als nahezu reiner Sinus auf der Grenzfrequenz, statt zu explodieren.
    struct MultimodeSvf
    {
        float ic1 = 0, ic2 = 0;
        float energy = 0;
        uint32_t seed = 22222;

        void reset() { ic1 = ic2 = energy = 0; }

        // Notbremse: exakt linear bis |x| = 2, darueber weiches Knie mit Grenze 4
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
            const float k = c.k + c.limit * energy;

            // Ein analoger Filter rauscht immer ein wenig - das stoesst die Selbstoszillation
            // auch ohne Eingangssignal an (ca. -100 dBFS, nur wenn der Filter schwingen kann).
            if (c.k < 0.0f)
            {
                seed = seed * 1664525u + 1013904223u;
                x += 1.0e-5f * ((float) (seed >> 9) * (1.0f / 8388608.0f) - 1.0f);
            }

            const float a1 = 1.0f / (1.0f + c.g * (c.g + k));
            const float a2 = c.g * a1;
            const float a3 = c.g * a2;

            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = sat (2.0f * v1 - ic1);
            ic2 = sat (2.0f * v2 - ic2);

            const float lp = v2, bp = v1, hp = x - k * v1 - v2;
            energy = bp * bp + lp * lp;

            switch ((int) c.type)
            {
                case Params::LP2: return lp;
                case Params::LP1: return lp + bp;              // bei RES 0 exakt 1-polig (6 dB/Okt)
                case Params::BP:  return bp * c.bpGain;
                case Params::HP1: return hp + bp;              // bei RES 0 exakt 1-polig
                case Params::HP2: return hp;
                case Params::BS:  return x - k * bp;           // Notch, bei RES 0 am breitesten
                case Params::PK:  return x + c.pkGain * c.k * bp;
                default:          return lp;
            }
        }
    };

    // Analytischer Betragsfrequenzgang des linearen Filters (fuer Display und Tests).
    // Im Selbstoszillationsbereich wird die Daempfung fuer die Darstellung auf Q 50 begrenzt.
    inline double magnitude (double f, double cutoffHz, double resValue, int type)
    {
        SvfCoeffs c;
        c.set (cutoffHz, resValue, type, 1.0e9); // praktisch analoger Prototyp
        const std::complex<double> s (0.0, f / cutoffHz);
        const double k = std::max ((double) c.k, 0.02);
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
    // Transienten-Erkennung: Ein Schlag ist ein ploetzlicher Pegelsprung. Verglichen wird der
    // aktuelle Pegel mit dem Pegel von vor 5 ms - so zaehlt jeder neue Schlag, auch wenn der
    // vorige noch ausklingt (schnelle 16tel/32tel) oder ein Dauerton darunter liegt.
    struct TransientDetector
    {
        void prepare (double sr)
        {
            sampleRate = sr;
            hpCoef   = (float) std::exp (-2.0 * pi * 150.0 / sr); // Hoehenanhebung ab ca. 150 Hz
            rmsCoef  = (float) std::exp (-1.0 / (0.001 * sr));   // 1 ms Glaettung (Rauschen beruhigen)
            relCoef  = (float) std::exp (-1.0 / (0.060 * sr));   // Pegel faellt mit 60 ms (wenig Welligkeit bei Bass)
            history.assign ((size_t) std::max (1, (int) std::lround (0.005 * sr)), 0.0f);
            minGap = std::max (1, (int) std::lround (0.025 * sr)); // fruehestens alle 25 ms (32tel bis 300 BPM)
            reset();
        }

        void reset()
        {
            power = level = hpState = 0.0f;
            std::fill (history.begin(), history.end(), 0.0f);
            pos = 0;
            holdoff = 0;
            armed = true;
        }

        // sens 0..127: hoeher = empfindlicher (kleinerer Sprung und leiserer Pegel reichen)
        bool process (float x, double sens)
        {
            if (history.empty())
                prepare (sampleRate);

            // Detektor hoert "heller": Anschlaege sind breitbandig, Basstoene nicht -> ein
            // durchgehender Bass verdeckt die Schlaege weniger
            hpState = x + (hpState - x) * hpCoef;
            const float d = x + 3.0f * (x - hpState);

            // Pegel: kurz geglaettetes RMS, dann Spitzenwert mit langsamem Abfall
            power = d * d + (power - d * d) * rmsCoef;
            const float rms = std::sqrt (power);
            level = rms > level ? rms : level * relCoef;

            const float before = history[pos];
            history[pos] = level;
            pos = (pos + 1) % history.size();

            if (sens != lastSens)
            {
                lastSens = sens;
                const double s = std::clamp (sens, 0.0, 127.0) / 127.0;
                jumpRatio  = (float) std::pow (10.0, (10.0 - 7.5 * s) / 20.0);   // Sprung 10 .. 2,5 dB
                rearmRatio = std::sqrt (jumpRatio);                              // halber Sprung in dB
                floorLevel = (float) std::pow (10.0, (-20.0 - 40.0 * s) / 20.0); // -20 .. -60 dBFS
            }

            if (holdoff > 0)
                --holdoff;

            // erst wieder scharf, wenn der Sprung abgeklungen ist (kein Doppel-Trigger)
            if (! armed && level < before * rearmRatio)
                armed = true;

            if (armed && holdoff == 0 && level > floorLevel && level > before * jumpRatio + 1.0e-9f)
            {
                armed = false;
                holdoff = minGap;
                return true;
            }
            return false;
        }

        double sampleRate = 44100.0;

    private:
        float hpCoef = 0, rmsCoef = 0, relCoef = 0;
        float hpState = 0, power = 0, level = 0;
        std::vector<float> history;
        size_t pos = 0;
        int minGap = 1, holdoff = 0;
        bool armed = true;
        double lastSens = -1.0;
        float jumpRatio = 2, rearmRatio = 1.4f, floorLevel = 0.001f;
    };
}
