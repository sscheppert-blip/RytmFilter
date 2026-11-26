#pragma once

#include <cmath>
#include <algorithm>

// Parameter-IDs und Wertebereiche. Alle Regler arbeiten wie am Original im Bereich 0..127
// (ENV bipolar -64..+63). Intern sind die Werte stufenlos, angezeigt wird ganzzahlig.
namespace Params
{
    // FLTR-Seite
    inline constexpr const char* atk = "atk";
    inline constexpr const char* dec = "dec";
    inline constexpr const char* sus = "sus";
    inline constexpr const char* rel = "rel";
    inline constexpr const char* frq = "frq";
    inline constexpr const char* res = "res";
    inline constexpr const char* typ = "typ";
    inline constexpr const char* env = "env";

    // TRIG-Seite (Ausloesung der Filterhuellkurve)
    inline constexpr const char* trigMode = "trigmode";
    inline constexpr const char* trigRate = "trigrate";
    inline constexpr const char* trigSens = "trigsens";
    inline constexpr const char* trigLen  = "triglen";

    enum FilterType { LP2 = 0, LP1, BP, HP1, HP2, BS, PK, numTypes };
    inline const char* typeShort (int t)
    {
        static const char* n[] = { "LP2", "LP1", "BP", "HP1", "HP2", "BS", "PK" };
        return n[std::clamp (t, 0, numTypes - 1)];
    }
    inline const char* typeLong (int t)
    {
        static const char* n[] = { "2-POLE LOWPASS", "1-POLE LOWPASS", "BANDPASS", "1-POLE HIGHPASS",
                                   "2-POLE HIGHPASS", "BANDSTOP", "PEAK" };
        return n[std::clamp (t, 0, numTypes - 1)];
    }

    enum TrigMode { MIDI = 0, TRANSIENT, SYNC, numModes };
    inline const char* modeShort (int m)
    {
        static const char* n[] = { "MIDI", "TRNS", "SYNC" };
        return n[std::clamp (m, 0, numModes - 1)];
    }

    inline constexpr int numRates = 8;
    inline const char* rateShort (int r)
    {
        static const char* n[] = { "1/32", "1/16", "1/8", "1/4", "1/2", "1BAR", "2BAR", "4BAR" };
        return n[std::clamp (r, 0, numRates - 1)];
    }
    inline double rateInQuarters (int r)
    {
        static const double q[] = { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
        return q[std::clamp (r, 0, numRates - 1)];
    }

    // FRQ 0..127 -> 20 Hz .. 20 kHz exponentiell (eine Stufe ~ 0,94 Halbtoene)
    inline double frqToHz (double v)  { return 20.0 * std::pow (1000.0, std::clamp (v, 0.0, 127.0) / 127.0); }
    inline double hzToFrq (double hz) { return 127.0 * std::log10 (std::max (hz, 20.0) / 20.0) / 3.0; }

    // Huellkurvenzeiten 0..127 -> 0 .. 10 s (kubisch, feine Aufloesung bei kurzen Zeiten)
    inline double envTimeMs (double v)
    {
        const double x = std::clamp (v, 0.0, 127.0) / 127.0;
        return v <= 0.0 ? 0.0 : 0.5 + 9999.5 * x * x * x;
    }

    // TRIG LEN 0..127 -> Gate-Laenge 5 ms .. 2 s
    inline double gateLenMs (double v) { return 5.0 * std::pow (400.0, std::clamp (v, 0.0, 127.0) / 127.0); }

    // RES 0..127 -> Guete Q 0.5 .. 30
    inline double resToQ (double v) { return 0.5 * std::pow (60.0, std::clamp (v, 0.0, 127.0) / 127.0); }
}
