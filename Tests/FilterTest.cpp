// DSP-Tests fuer RytmFilter: Mapping, Filterkennlinien, Stabilitaet, Huellkurve, Transienten.
#include <juce_core/juce_core.h>
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
    check (std::abs (Params::frqToHz (0) - 20.0) < 1e-9, "FRQ 0 = 20 Hz", Params::frqToHz (0), 20.0);
    check (std::abs (Params::frqToHz (127) - 20000.0) < 1e-6, "FRQ 127 = 20 kHz", Params::frqToHz (127), 20000.0);
    check (std::abs (Params::hzToFrq (Params::frqToHz (63.5)) - 63.5) < 1e-9, "hzToFrq ist Umkehrung", Params::hzToFrq (Params::frqToHz (63.5)), 63.5);

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
    check (std::abs (db (dsp::magnitude (fc, fc, 127, Params::BP)) - 12.04) < 0.1, "BP bei fc, RES 127: +12 dB", db (dsp::magnitude (fc, fc, 127, Params::BP)), 12.04);
    check (std::abs (db (dsp::magnitude (fc, fc, 127, Params::PK)) - 24.0) < 0.1, "PK bei fc, RES 127: +24 dB", db (dsp::magnitude (fc, fc, 127, Params::PK)), 24.0);
    check (std::abs (db (dsp::magnitude (fc, fc, 0, Params::PK))) < 0.01, "PK bei RES 0: neutral", db (dsp::magnitude (fc, fc, 0, Params::PK)), 0.0);
    check (db (dsp::magnitude (fc, fc, 127, Params::LP2)) > 25.0, "LP2 RES 127: Resonanzspitze > +25 dB", db (dsp::magnitude (fc, fc, 127, Params::LP2)), 29.5);

    std::printf ("Diskreter Filter (2x 48 kHz) gegen Analytik, leiser Pegel\n");
    const double fs = 96000.0;
    for (int type = 0; type < Params::numTypes; ++type)
        for (double res : { 0.0, 64.0, 110.0 })
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
        dsp::TransientDetector det;
        det.sampleRate = 48000.0;
        std::mt19937 rng (3);
        std::normal_distribution<float> noise (0.0f, 0.003f);
        int hits = 0;
        const int numHits = 8;
        for (int i = 0; i < 48000 * 4; ++i)
        {
            float x = noise (rng);
            const int t = i % 24000; // alle 0,5 s ein Schlag
            if (t < 4800)
                x += 0.6f * std::exp (-t / 600.0f) * (float) std::sin (2.0 * dsp::pi * 80.0 * t / 48000.0);
            if (det.process (x, 64.0)) ++hits;
        }
        check (hits == numHits, "8 Schlaege in 4 s erkannt", hits, numHits);
    }

    std::printf (failures == 0 ? "\nALLE TESTS OK\n" : "\n%d FEHLER\n", failures);
    return failures == 0 ? 0 : 1;
}
