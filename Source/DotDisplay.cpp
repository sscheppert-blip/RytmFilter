#include "DotDisplay.h"
#include "FilterDSP.h"
#include "Params.h"

#include <cmath>

namespace
{
    std::string fmt (const char* f, double v)
    {
        char buf[32];
        std::snprintf (buf, sizeof (buf), f, v);
        return buf;
    }

    std::string formatHz (double hz)
    {
        if (hz < 100.0)   return fmt ("%.1fHZ", hz);
        if (hz < 1000.0)  return fmt ("%.0fHZ", hz);
        if (hz < 10000.0) return fmt ("%.2fKHZ", hz / 1000.0);
        return fmt ("%.1fKHZ", hz / 1000.0);
    }

    std::string formatInt (double v) { return std::to_string ((int) std::lround (v)); }

    std::string formatMs (double ms)
    {
        if (ms < 100.0) return fmt ("%.0fMS", ms);
        return fmt ("%.2fS", ms / 1000.0);
    }

    constexpr int colX[4] = { 16, 48, 80, 112 }; // Spaltenmitten fuer 4 Encoder pro Reihe
}

DotDisplay::DotDisplay (RytmFilterProcessor& p) : proc (p)
{
    setOpaque (true);
}

float DotDisplay::param (const char* id) const
{
    return proc.apvts.getRawParameterValue (id)->load();
}

void DotDisplay::showTouched (int slot)
{
    touchedSlot = slot;
    touchedMs = juce::Time::getMillisecondCounterHiRes();
}

void DotDisplay::refresh()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (touchedSlot >= 0 && now - touchedMs > 1500.0)
        touchedSlot = -1;

    const int tc = proc.live.trigCount.load();
    if (tc != lastTrigCount)
    {
        lastTrigCount = tc;
        trigFlashMs = now;
    }

    canvas.clear();
    if (page == Page::filter)
        renderFilterPage();
    else
        renderTrigPage();
    renderFooter();
    repaint();
}

//==============================================================================
void DotDisplay::labelSlot (int slot, int y, const std::string& label, const std::string& value)
{
    const int cx = colX[slot % 4];
    if (slot == touchedSlot)
    {
        canvas.fillRect (cx - 15, y - 1, 31, 9);
        canvas.textCentred (cx, y, value, false);
    }
    else
    {
        canvas.textCentred (cx, y, label);
    }
}

void DotDisplay::headerBox (const std::string& text)
{
    const int tw = PixelCanvas::textWidth (text);
    const int x0 = 127 - tw - 4;
    canvas.rect (x0, 0, 128 - x0, 9);
    canvas.text (x0 + 2, 1, text);
}

void DotDisplay::dither (int x, int y, int w, int h)
{
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx)
            if (((xx + yy) & 1) != 0)
                canvas.set (xx, yy, false);
}

void DotDisplay::drawEnvelopeGraph (int y0, int y1)
{
    const double atk = param (Params::atk), dec = param (Params::dec);
    const double sus = param (Params::sus), rel = param (Params::rel);

    // Phasenbreiten (Attack/Decay/Release proportional, Sustain fest)
    double wA = 1.0 + 30.0 * std::sqrt (atk / 127.0);
    double wD = 1.0 + 30.0 * std::sqrt (dec / 127.0);
    double wS = 24.0;
    double wR = 1.0 + 30.0 * std::sqrt (rel / 127.0);
    const double total = wA + wD + wS + wR;
    const double scale = total > 120.0 ? 120.0 / total : 1.0;
    wA *= scale; wD *= scale; wS *= scale; wR *= scale;

    const int x0 = 3;
    const int xA = x0 + (int) std::lround (wA);
    const int xD = xA + (int) std::lround (wD);
    const int xS = xD + (int) std::lround (wS);
    const int xR = xS + (int) std::lround (wR);

    auto yFor = [&] (double level) { return y1 - (int) std::lround (level * (y1 - y0)); };
    const double susL = sus / 127.0;

    // Attack (linear)
    canvas.line (x0, y1, xA, y0);
    // Decay (exponentiell)
    int px = xA, py = y0;
    for (int x = xA + 1; x <= xD; ++x)
    {
        const double t = (double) (x - xA) / juce::jmax (1, xD - xA);
        const double l = susL + (1.0 - susL) * std::exp (-5.0 * t);
        const int y = yFor (x == xD ? susL : l);
        canvas.line (px, py, x, y);
        px = x; py = y;
    }
    // Sustain
    canvas.line (xD, yFor (susL), xS, yFor (susL));
    // Release
    px = xS; py = yFor (susL);
    for (int x = xS + 1; x <= xR; ++x)
    {
        const double t = (double) (x - xS) / juce::jmax (1, xR - xS);
        const double l = susL * std::exp (-5.0 * t);
        const int y = yFor (x == xR ? 0.0 : l);
        canvas.line (px, py, x, y);
        px = x; py = y;
    }

    // Phasengrenzen gepunktet + Knoten
    for (int x : { xA, xD, xS })
        canvas.dottedV (x, y0, y1);
    for (auto [x, y] : { std::pair<int, int> { x0, y1 }, { xA, y0 }, { xD, yFor (susL) }, { xS, yFor (susL) }, { xR, y1 } })
        canvas.rect (x - 1, y - 1, 3, 3);

}

void DotDisplay::drawResponse (int x0, int y0, int w, int h, double cutoffHz, double res, int type, bool marker)
{
    // 20 Hz .. 20 kHz logarithmisch, +24 .. -30 dB
    constexpr double topDb = 24.0, bottomDb = -30.0;
    auto yForFreq = [&] (double f)
    {
        const double db = 20.0 * std::log10 (std::max (1.0e-6, dsp::magnitude (f, cutoffHz, res, type)));
        const double t  = (topDb - std::clamp (db, bottomDb, topDb)) / (topDb - bottomDb);
        return y0 + (int) std::lround (t * (h - 1));
    };

    // pro Spalte mehrfach abtasten, damit schmale Notches/Spitzen nicht verschwinden
    int prevY = -1;
    for (int i = 0; i < w; ++i)
    {
        int yMin = y0 + h, yMax = y0 - 1, yMid = 0;
        for (int sub = 0; sub <= 8; ++sub)
        {
            const double pos = juce::jlimit (0.0, (double) (w - 1), i - 0.5 + sub / 8.0);
            const int y = yForFreq (20.0 * std::pow (1000.0, pos / (w - 1)));
            yMin = std::min (yMin, y);
            yMax = std::max (yMax, y);
            if (sub == 4) yMid = y;
        }
        canvas.vline (x0 + i, yMin, yMax);
        if (prevY >= 0)
            canvas.line (x0 + i - 1, prevY, x0 + i, yMid);
        prevY = yMid;
    }

    if (marker)
    {
        const int mx = x0 + (int) std::lround (std::log10 (std::clamp (cutoffHz, 20.0, 20000.0) / 20.0) / 3.0 * (w - 1));
        canvas.dottedV (mx, y0, y0 + h - 1);
    }
}

void DotDisplay::drawEnvKnob (int cx, int cy, double env)
{
    canvas.circle (cx, cy, 5);
    // bipolar: 0 = Norden, -64 .. +63 => -135 .. +135 Grad
    const double a = (env / 64.0) * 2.356;
    canvas.line (cx, cy, cx + (int) std::lround (std::sin (a) * 4.0), cy - (int) std::lround (std::cos (a) * 4.0));
    canvas.set (cx, cy - 7); // Mittenmarke
}

//==============================================================================
void DotDisplay::renderFilterPage()
{
    const double hz = proc.live.cutoffHz.load();

    // Kopfzeile: Seitenname + Live-Frequenz (wo der Filter gerade greift)
    canvas.text (1, 1, "FILTER");
    headerBox ("F:" + formatHz (hz));

    // Huellkurve
    drawEnvelopeGraph (12, 22);
    labelSlot (0, 25, "ATK", formatInt (param (Params::atk)));
    labelSlot (1, 25, "DEC", formatInt (param (Params::dec)));
    labelSlot (2, 25, "SUS", formatInt (param (Params::sus)));
    labelSlot (3, 25, "REL", formatInt (param (Params::rel)));

    // Frequenzgang (FRQ/RES-Spalten), Typ-Symbol, ENV-Knopf
    const int type = juce::roundToInt (param (Params::typ));
    const double res = param (Params::res);
    drawResponse (1, 34, 62, 11, hz, res, type, true);
    drawResponse (68, 35, 25, 9, 632.0, juce::jmax (40.0, res), type, false);
    drawEnvKnob (112, 39, param (Params::env));

    labelSlot (4, 46, "FRQ", formatInt (param (Params::frq)));
    labelSlot (5, 46, "RES", formatInt (param (Params::res)));
    labelSlot (6, 46, "TYP", Params::typeShort (type));
    labelSlot (7, 46, "ENV", formatInt (param (Params::env)));
}

void DotDisplay::renderTrigPage()
{
    const int mode = juce::roundToInt (param (Params::trigMode));
    const int rate = juce::roundToInt (param (Params::trigRate));

    canvas.text (1, 1, "TRIG SETUP");
    headerBox (fmt ("BPM:%.1f", proc.live.bpm.load()));

    const std::string values[4] = {
        Params::modeShort (mode),
        Params::rateShort (rate),
        formatInt (param (Params::trigSens)),
        formatMs (Params::gateLenMs (param (Params::trigLen))),
    };
    const char* labels[4] = { "MOD", "RAT", "SNS", "LEN" };
    const bool active[4] = { true, mode == Params::SYNC, mode == Params::TRANSIENT, mode != Params::MIDI };

    for (int i = 0; i < 4; ++i)
    {
        const int cx = colX[i];
        if (active[i])
            canvas.rect (cx - 15, 12, 31, 11);
        else
        {
            // inaktiv im aktuellen Modus: gepunkteter Rahmen
            canvas.dottedH (cx - 15, cx + 15, 12); canvas.dottedH (cx - 15, cx + 15, 22);
            canvas.dottedV (cx - 15, 12, 22);      canvas.dottedV (cx + 15, 12, 22);
        }
        canvas.textCentred (cx, 14, values[i]);
        labelSlot (i, 25, labels[i], values[i]);
    }

    canvas.dottedH (0, 127, 34);
    switch (mode)
    {
        case Params::MIDI:
            canvas.text (1, 37, "NOTE ON  = TRIG");
            canvas.text (1, 46, "NOTE OFF = RELEASE");
            break;
        case Params::TRANSIENT:
            canvas.text (1, 37, "AUDIO HIT = TRIG");
            canvas.text (1, 46, "LEN = GATE TIME");
            break;
        default:
            canvas.text (1, 37, std::string ("EVERY ") + Params::rateShort (rate));
            canvas.text (1, 46, proc.live.playing.load() ? "HOST PLAYING" : "HOST STOPPED");
            break;
    }
}

void DotDisplay::renderFooter()
{
    canvas.hline (0, 127, 54);
    const int mode = juce::roundToInt (param (Params::trigMode));
    canvas.text (1, 56, std::string ("TRG:") + Params::modeShort (mode));

    // Huellkurvenpegel als Balken (wie die Level-Anzeige am Geraet)
    canvas.text (52, 56, "ENV");
    canvas.rect (71, 56, 42, 7);
    const int w = (int) std::lround (proc.live.envLevel.load() * 40.0);
    canvas.fillRect (72, 57, w, 5);
    for (int x = 75; x < 112; x += 5) // Skala im Balken
        canvas.invert (x, 59);

    // Trig-Anzeige: blinkt bei jedem Trigger, voll bei offenem Gate
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (proc.live.gateOpen.load() || now - trigFlashMs < 80.0)
        canvas.fillRect (120, 56, 7, 7);
    else
        canvas.rect (120, 56, 7, 7);
}

//==============================================================================
void DotDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff050505));
    g.fillRect (b);

    const float cell = juce::jmin (b.getWidth() / PixelCanvas::W, b.getHeight() / PixelCanvas::H);
    const float dot  = cell * 0.84f;
    const float ox   = b.getX() + (b.getWidth()  - cell * PixelCanvas::W) * 0.5f;
    const float oy   = b.getY() + (b.getHeight() - cell * PixelCanvas::H) * 0.5f;

    juce::RectangleList<float> lit, unlit;
    lit.ensureStorageAllocated (4096);
    unlit.ensureStorageAllocated (8192);

    for (int y = 0; y < PixelCanvas::H; ++y)
        for (int x = 0; x < PixelCanvas::W; ++x)
        {
            const juce::Rectangle<float> r (ox + x * cell, oy + y * cell, dot, dot);
            if (canvas.get (x, y)) lit.addWithoutMerging (r);
            else                   unlit.addWithoutMerging (r);
        }

    g.setColour (juce::Colour (0xff141414)); // unbeleuchtete Punkte, gerade noch sichtbar
    g.fillRectList (unlit);

    g.setColour (juce::Colour (0xffeef2f0).withAlpha (0.35f)); // leichter Glow
    for (const auto& r : lit)
        g.fillRect (r.expanded (cell * 0.12f));
    g.setColour (juce::Colour (0xfff4f7f5));
    g.fillRectList (lit);

    // Glas-Reflex
    juce::ColourGradient glare (juce::Colours::white.withAlpha (0.045f), b.getX(), b.getY(),
                                juce::Colours::transparentWhite, b.getX(), b.getCentreY(), false);
    g.setGradientFill (glare);
    g.fillRect (b.withHeight (b.getHeight() * 0.5f));
}
