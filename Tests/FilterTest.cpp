// DSP-Tests fuer RytmFilter: Mapping, Filterkennlinien, Stabilitaet, Huellkurve, Transienten.
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "FilterDSP.h"
#include "Params.h"
#include <cstdio>
#include <random>
#include <vector>

static int failures = 0;

static void check (bool ok, const char* what, double got, double expected)
{
    std::printf ("  %-58s %s  (ist %.3f, soll %.3f)\n", what, ok ? "OK  " : "FAIL", got, expected);
    if (! ok) ++failures;
}

static double db (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

// Gemessene Verstaerkung eines Sinus durch den diskreten Filter (Oversampling-Rate)
static double measureGain (int type, double cutoff, double res, double freq, double fs, float amp = 0.01f)
{
    dsp::MultimodeSvf f;
    dsp::SvfCoeffs c;
    c.set (cutoff, res, type, fs);
    const int settle = (int) (fs * 0.5), n = (int) (fs * 0.5);
    double inE = 0, outE = 0;
    for (int i = 0; i < settle + n; ++i)
    {
        const float x = amp * (float) std::sin (2.0 * dsp::pi * freq * i / fs);
        const float y = f.process (x, c);
        if (i >= settle) { inE += (double) x * x; outE += (double) y * y; }
    }
    return std::sqrt (outE / inE);
}

int main()
{
    std::printf ("Mapping\n");
    check (std::abs (Params::frqToHz (60) - 440.0) < 1e-9, "FRQ 60 = 440 Hz (A4)", Params::frqToHz (60), 440.0);
    check (std::abs (Params::frqToHz (72) - 880.0) < 1e-9, "FRQ 72 = 880 Hz (eine Oktave = 12 Stufen)", Params::frqToHz (72), 880.0);
    check (std::abs (Params::frqToHz (0) - 13.75) < 1e-9, "FRQ 0 = 13,75 Hz", Params::frqToHz (0), 13.75);
    check (std::abs (Params::frqToHz (127) - 21096.2) < 0.5, "FRQ 127 = 21,1 kHz", Params::frqToHz (127), 21096.2);
    check (std::abs (Params::hzToFrq (Params::frqToHz (63.5)) - 63.5) < 1e-9, "hzToFrq ist Umkehrung", Params::hzToFrq (Params::frqToHz (63.5)), 63.5);
    {
        // v1.0-Projekte: alter Wert -> naechster Halbton derselben Frequenz
        double worst = 0.0;
        for (int v = 0; v <= 127; ++v)
        {
            const double hzOld = Params::legacyFrqToHz (v);
            const double hzNew = Params::frqToHz (std::round (Params::hzToFrq (hzOld)));
            worst = std::max (worst, std::abs (12.0 * std::log2 (hzNew / hzOld)));
        }
        check (worst <= 0.5 + 1e-9, "v1.0-Umrechnung: max. Abweichung in Halbtoenen", worst, 0.5);
    }
    check (Params::resToK (95) > 0.0 && Params::resToK (97) < 0.0, "RES: Selbstoszillation ab 96", Params::resToK (97), 0.0);

    std::printf ("Analytische Kennlinien (RES 0, Abweichung in dB)\n");
    const double fc = 1000.0;
    check (std::abs (db (dsp::magnitude (fc, fc, 0, Params::LP1)) + 3.01) < 0.05, "LP1 bei fc: -3 dB (1-polig)", db (dsp::magnitude (fc, fc, 0, Params::LP1)), -3.01);
    check (std::abs (db (dsp::magnitude (100 * fc, fc, 0, Params::LP1)) + 40.0) < 0.1, "LP1 100x fc: -40 dB (6 dB/Okt)", db (dsp::magnitude (100 * fc, fc, 0, Params::LP1)), -40.0);
    check (std::abs (db (dsp::magnitude (100 * fc, fc, 0, Params::LP2)) + 80.0) < 0.2, "LP2 100x fc: -80 dB (12 dB/Okt)", db (dsp::magnitude (100 * fc, fc, 0, Params::LP2)), -80.0);
    check (std::abs (db (dsp::magnitude (fc / 100, fc, 0, Params::HP1)) + 40.0) < 0.1, "HP1 fc/100: -40 dB", db (dsp::magnitude (fc / 100, fc, 0, Params::HP1)), -40.0);
    check (std::abs (db (dsp::magnitude (fc / 100, fc, 0, Params::HP2)) + 80.0) < 0.2, "HP2 fc/100: -80 dB", db (dsp::magnitude (fc / 100, fc, 0, Params::HP2)), -80.0);
    check (dsp::magnitude (fc, fc, 0, Params::BS) < 1e-6, "BS bei fc: Notch (Betrag ~0)", dsp::magnitude (fc, fc, 0, Params::BS), 0.0);
    check (db (dsp::magnitude (fc * 0.7, fc, 0, Params::BS)) < db (dsp::magnitude (fc * 0.7, fc, 127, Params::BS)) - 3.0,
           "BS: Notch bei RES 0 breiter als bei RES 127", db (dsp::magnitude (fc * 0.7, fc, 0, Params::BS)), db (dsp::magnitude (fc * 0.7, fc, 127, Params::BS)));
    check (std::abs (db (dsp::magnitude (fc, fc, 0, Params::BP))) < 0.05, "BP bei fc, RES 0: 0 dB", db (dsp::magnitude (fc, fc, 0, Params::BP)), 0.0);
    {
        const double want = db (1.0 + 3.0 * 88.0 / 127.0);
        check (std::abs (db (dsp::magnitude (fc, fc, 88, Params::BP)) - want) < 0.1, "BP bei fc, RES 88: ca. +10 dB", db (dsp::magnitude (fc, fc, 88, Params::BP)), want);
    }
    check (std::abs (db (dsp::magnitude (fc, fc, 127, Params::PK)) - 24.0) < 0.1, "PK bei fc, RES 127: +24 dB", db (dsp::magnitude (fc, fc, 127, Params::PK)), 24.0);
    check (std::abs (db (dsp::magnitude (fc, fc, 0, Params::PK))) < 0.01, "PK bei RES 0: neutral", db (dsp::magnitude (fc, fc, 0, Params::PK)), 0.0);
    check (db (dsp::magnitude (fc, fc, 127, Params::LP2)) > 25.0, "LP2 RES 127: Resonanzspitze > +25 dB", db (dsp::magnitude (fc, fc, 127, Params::LP2)), 29.5);

    std::printf ("Diskreter Filter (2x 48 kHz) gegen Analytik, leiser Pegel\n");
    const double fs = 96000.0;
    for (int type = 0; type < Params::numTypes; ++type)
        for (double res : { 0.0, 64.0, 80.0 }) // darueber arbeitet der Filter bewusst nichtlinear
            for (double f : { 200.0, 1000.0, 5000.0 })
            {
                const double meas = db (measureGain (type, fc, res, f, fs));
                const double ana  = db (dsp::magnitude (f, fc, res, type));
                if (ana < -60.0) continue; // Notch-Tiefe ist messtechnisch irrelevant
                char what[96];
                std::snprintf (what, sizeof (what), "%s RES %3.0f bei %5.0f Hz", Params::typeShort (type), res, f);
                check (std::abs (meas - ana) < 0.5, what, meas, ana);
            }

    std::printf ("Stabilitaet bei RES 127, lautes Rauschen + Sweep\n");
    {
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> noise (-2.0f, 2.0f);
        for (int type = 0; type < Params::numTypes; ++type)
        {
            dsp::MultimodeSvf f;
            dsp::SvfCoeffs c;
            double peak = 0.0;
            bool finite = true;
            for (int i = 0; i < (int) fs * 5; ++i)
            {
                const double frq = 63.5 + 63.5 * std::sin (2.0 * dsp::pi * 3.0 * i / fs); // schneller Sweep ueber alles
                c.set (Params::frqToHz (frq), 127.0, type, fs);
                const float y = f.process (noise (rng), c);
                finite = finite && std::isfinite (y);
                peak = std::max (peak, (double) std::abs (y));
            }
            char what[96];
            std::snprintf (what, sizeof (what), "%s: endlich und begrenzt (Spitze < 40)", Params::typeShort (type));
            check (finite && peak < 40.0, what, peak, 40.0);
        }
    }

    std::printf ("Selbstoszillation (ohne Eingangssignal, 2x 48 kHz)\n");
    {
        auto oscillate = [&] (double res, int type, double cutoff, double& freq)
        {
            dsp::MultimodeSvf f;
            dsp::SvfCoeffs c;
            c.set (cutoff, res, type, fs);
            const int n = (int) fs * 3, tail = (int) fs;
            double peak = 0.0;
            int crossings = 0;
            float prev = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float y = f.process (0.0f, c);
                if (i >= n - tail)
                {
                    peak = std::max (peak, (double) std::abs (y));
                    if (prev <= 0.0f && y > 0.0f) ++crossings;
                }
                prev = y;
            }
            freq = crossings * fs / tail;
            return peak;
        };

        double freq = 0.0;
        const double p90 = oscillate (90, Params::LP2, 440.0, freq);
        check (p90 < 1.0e-3, "RES 90: schwingt nicht von selbst", p90, 0.0);

        const double p127 = oscillate (127, Params::LP2, 440.0, freq);
        check (p127 > 0.3 && p127 < 0.8, "RES 127: Sinus mit ca. -6 dBFS", p127, 0.5);
        check (std::abs (12.0 * std::log2 (std::max (freq, 1.0) / 440.0)) < 0.5, "RES 127: schwingt auf FRQ 60 = 440 Hz (+/- Halbton)", freq, 440.0);

        const double p105 = oscillate (105, Params::LP2, 2000.0, freq);
        check (p105 > 0.1 && p105 < p127, "RES 105: leiser als RES 127", p105, p127);

        const double hp = oscillate (127, Params::HP2, 1000.0, freq);
        check (hp > 0.3 && hp < 0.8, "HP2 RES 127: schwingt ebenfalls", hp, 0.5);
    }

    std::printf ("Oversampling-Latenz (wie im Plugin: 2x, Halbband-IIR)\n");
    {
        juce::dsp::Oversampling<float> os (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os.initProcessing (512);
        const double lat = os.getLatencyInSamples();
        check (lat < 5.0, "Latenz unter 5 Samples (ca. 0,07 ms bei 44,1 kHz)", lat, 3.0);
    }

    std::printf ("Huellkurve\n");
    {
        dsp::Envelope env;
        env.sampleRate = 48000.0;
        env.gateOn();
        int n = 0;
        while (env.stage == dsp::Envelope::attack && n < 48000 * 20) { env.process (64, 64, 64, 64); ++n; }
        const double ms = n / 48.0;
        check (std::abs (ms - Params::envTimeMs (64)) < 2.0, "ATK 64: Attackzeit in ms", ms, Params::envTimeMs (64));

        for (int i = 0; i < 48000 * 10; ++i) env.process (64, 64, 64, 64);
        check (std::abs (env.level - 64.0 / 127.0) < 1e-3, "SUS 64: Haltepegel", env.level, 64.0 / 127.0);

        env.gateOff();
        n = 0;
        while (env.stage != dsp::Envelope::idle && n < 48000 * 30) { env.process (64, 64, 64, 32); ++n; }
        const double relMs = n / 48.0;
        // Release-Zeit ist bis -60 dB definiert, Abbruch bei 1e-4 (~ -74 dB relativ zu 0,5)
        check (relMs > Params::envTimeMs (32) * 0.9 && relMs < Params::envTimeMs (32) * 1.4, "REL 32: Ausklingzeit in ms", relMs, Params::envTimeMs (32));

        dsp::Envelope e0;
        e0.sampleRate = 48000.0;
        e0.gateOn();
        e0.process (0, 0, 127, 0);
        check (e0.level >= 1.0, "ATK 0: sofort voll", e0.level, 1.0);
    }

    std::printf ("Transienten-Erkennung\n");
    {
        constexpr double sr = 48000.0;

        // Ein "Schlag": Sinus mit exponentiellem Ausklang (decayMs = Zeit bis -60 dB)
        struct Hit { double t, amp, freq, decayMs; };

        // Zaehlt Trigger; signal = Schlaege + optionales Bett (Dauerton) + leises Rauschen
        auto countHits = [&] (const std::vector<Hit>& hits, double seconds, double sens,
                              double bedAmp = 0.0, double bedFreq = 55.0)
        {
            dsp::TransientDetector det;
            det.prepare (sr);
            std::mt19937 rng (3);
            std::normal_distribution<float> noise (0.0f, 0.002f);
            int n = 0;
            for (int i = 0; i < (int) (sr * seconds); ++i)
            {
                const double t = i / sr;
                double x = noise (rng) + bedAmp * std::sin (2.0 * dsp::pi * bedFreq * t);
                for (const auto& h : hits)
                    if (t >= h.t && t < h.t + h.decayMs * 0.001)
                    {
                        const double u = t - h.t;
                        x += h.amp * std::exp (-6.9 * u / (h.decayMs * 0.001)) * std::sin (2.0 * dsp::pi * h.freq * u);
                    }
                if (det.process ((float) x, sens) && t > 0.04) ++n; // Signalstart bei t=0 nicht zaehlen
            }
            return n;
        };

        auto grid = [] (int count, double stepSec, double amp, double freq, double decayMs)
        {
            std::vector<Hit> v;
            for (int i = 0; i < count; ++i) v.push_back ({ 0.05 + i * stepSec, amp, freq, decayMs });
            return v;
        };

        // Bisheriger Fall: einzelne Kicks alle 0,5 s
        check (countHits (grid (8, 0.5, 0.6, 80.0, 90.0), 4.0, 64.0) == 8, "Kicks alle 0,5 s: 8 von 8", countHits (grid (8, 0.5, 0.6, 80.0, 90.0), 4.0, 64.0), 8);

        // 16tel bei 128 BPM, gleich laut, langer Ausklang (laeuft in den naechsten Schlag)
        const double s16 = 60.0 / 128.0 / 4.0;
        {
            const int got = countHits (grid (16, s16, 0.5, 120.0, 300.0), 2.2, 64.0);
            check (got == 16, "16tel @128 BPM, Ausklang 300 ms: 16 von 16", got, 16);
        }
        // 16tel bei 174 BPM (Drum & Bass), kurzer Ausklang
        {
            const double st = 60.0 / 174.0 / 4.0;
            const int got = countHits (grid (16, st, 0.5, 200.0, 60.0), 1.6, 64.0);
            check (got == 16, "16tel @174 BPM: 16 von 16", got, 16);
        }
        // 16tel bei 150 BPM, gleich laut, Ausklang laeuft in den naechsten Schlag
        {
            const int got = countHits (grid (16, 60.0 / 150.0 / 4.0, 0.5, 120.0, 250.0), 1.8, 64.0);
            check (got == 16, "16tel @150 BPM, Ausklang 250 ms: 16 von 16", got, 16);
        }
        // 32tel bei 150 BPM
        {
            const int got = countHits (grid (16, 60.0 / 150.0 / 8.0, 0.5, 300.0, 40.0), 1.0, 64.0);
            check (got == 16, "32tel @150 BPM: 16 von 16", got, 16);
        }
        // 32tel bei 120 BPM
        {
            const int got = countHits (grid (16, 60.0 / 120.0 / 8.0, 0.5, 300.0, 40.0), 1.2, 64.0);
            check (got == 16, "32tel @120 BPM: 16 von 16", got, 16);
        }
        // Muster 0-1-1-1: lauter Schlag, dann drei leisere 16tel
        {
            std::vector<Hit> v;
            const double s150 = 60.0 / 150.0 / 4.0;
            for (int bar = 0; bar < 4; ++bar)
                for (int k = 0; k < 4; ++k)
                    v.push_back ({ 0.05 + (bar * 4 + k) * s150, k == 0 ? 0.7 : 0.35, 150.0, 200.0 });
            const int got = countHits (v, 1.8, 64.0);
            check (got == 16, "Muster 0-1-1-1 @150 BPM (1 laut, 3 leiser): 16 von 16", got, 16);
        }
        // 16tel ueber einem durchgehenden Basston (Bett -12 dB)
        {
            const int got = countHits (grid (16, s16, 0.5, 180.0, 80.0), 2.2, 64.0, 0.25, 55.0);
            check (got == 16, "16tel ueber Dauer-Basston: 16 von 16", got, 16);
        }
        // Kein Doppel-Trigger: ein Kick mit langem 45-Hz-Ausklang = genau 1
        {
            const int got = countHits ({ { 0.05, 0.8, 45.0, 600.0 } }, 1.0, 127.0);
            check (got == 1, "Langer 45-Hz-Kick, SNS 127: genau 1 Trigger", got, 1);
        }
        // Dauerton allein loest nichts aus
        {
            const int got = countHits ({}, 2.0, 127.0, 0.4, 55.0);
            check (got == 0, "Dauerton ohne Schlaege: 0 Trigger", got, 0);
        }
        // Empfindlichkeit: leise Schlaege (-34 dBFS) nur bei hohem SNS
        {
            const int low  = countHits (grid (8, 0.25, 0.02, 2000.0, 30.0), 2.2, 0.0);
            const int high = countHits (grid (8, 0.25, 0.02, 2000.0, 30.0), 2.2, 127.0);
            check (low == 0, "Leise Schlaege bei SNS 0: ignoriert", low, 0);
            check (high == 8, "Leise Schlaege bei SNS 127: 8 von 8", high, 8);
        }
    }

    std::printf (failures == 0 ? "\nALLE TESTS OK\n" : "\n%d FEHLER\n", failures);
    return failures == 0 ? 0 : 1;
}
