#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PixelCanvas.h"
#include "PluginProcessor.h"

// 128x64-Punktmatrix-Display. Jedes Pixel wird als Vektor-Rechteck gezeichnet -> bleibt
// in jeder Skalierung scharf, die "Dot"-Struktur ist sichtbar.
class DotDisplay : public juce::Component
{
public:
    enum class Page { filter, trig };

    explicit DotDisplay (RytmFilterProcessor&);

    void setPage (Page p) { page = p; }
    Page getPage() const  { return page; }

    // Slot 0..7 = Encoder A..H; zeigt kurz den Wert im Label-Feld
    void showTouched (int slot);

    void refresh(); // Framebuffer neu aufbauen und neu zeichnen
    void paint (juce::Graphics&) override;

private:
    void renderFilterPage();
    void renderTrigPage();
    void renderFooter();
    void drawEnvelopeGraph (int y0, int y1);
    void drawResponse (int x0, int y0, int w, int h, double cutoffHz, double res, int type, bool marker);
    void drawEnvKnob (int cx, int cy, double env);
    void labelSlot (int slot, int y, const std::string& label, const std::string& value);
    void dither (int x, int y, int w, int h);
    void headerBox (const std::string& text);

    float param (const char* id) const;

    RytmFilterProcessor& proc;
    PixelCanvas canvas;
    Page page = Page::filter;

    int touchedSlot = -1;
    double touchedMs = 0.0;
    int lastTrigCount = 0;
    double trigFlashMs = 0.0;
};
