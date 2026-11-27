#include "MainPanel.h"
#include "Params.h"

namespace
{
    constexpr int knobSize = 92;
    constexpr float trigLedX = 224.0f, trigLedY = 372.0f; // TRIG-IN-LED neben den Seitentasten
    constexpr int knobCols[4] = { 607, 717, 827, 937 };
    constexpr int knobRows[2] = { 128, 270 };
    const char* const knobLetters = "ABCDEFGH";

    const char* const filterIds[8] = { Params::atk, Params::dec, Params::sus, Params::rel,
                                       Params::frq, Params::res, Params::typ, Params::env };
    const char* const trigIds[8]   = { Params::trigMode, Params::trigRate, Params::trigSens, Params::trigLen,
                                       nullptr, nullptr, nullptr, nullptr };
}

MainPanel::MainPanel (RytmFilterProcessor& p) : proc (p), display (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (display);

    for (int i = 0; i < 8; ++i)
    {
        auto& e = encoders[(size_t) i];
        e.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        e.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        // Endlos-Encoder: keine Positionsanzeige am Knopf, der Wert steht im Display
        e.setMouseDragSensitivity (300);
        // Shift: Velocity-Modus fuer Feineinstellung
        e.setVelocityModeParameters (0.35, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        e.onDragStart   = [this, i] { display.showTouched (i); };
        e.onValueChange = [this, i]
        {
            auto& s = encoders[(size_t) i];
            if (s.isMouseButtonDown() || s.isMouseOver())
                display.showTouched (i);
        };
        addAndMakeVisible (e);
    }

    fltrKey.onClick = [this] { setPage (DotDisplay::Page::filter); };
    trigKey.onClick = [this] { setPage (DotDisplay::Page::trig); };
    addAndMakeVisible (fltrKey);
    addAndMakeVisible (trigKey);

    setPage (DotDisplay::Page::filter);
    setSize (baseWidth, baseHeight);
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
    for (auto& a : attachments)
        a.reset();
    setLookAndFeel (nullptr);
}

void MainPanel::setPage (DotDisplay::Page page)
{
    display.setPage (page);
    const auto* ids = page == DotDisplay::Page::filter ? filterIds : trigIds;

    for (int i = 0; i < 8; ++i)
    {
        auto& e = encoders[(size_t) i];
        attachments[(size_t) i].reset();
        e.setDoubleClickReturnValue (false, 0.0);

        if (auto* param = ids[i] != nullptr ? proc.apvts.getParameter (ids[i]) : nullptr)
        {
            e.setEnabled (true);
            attachments[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], e);
            e.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        }
        else
        {
            e.setEnabled (false);
            e.setRange (0.0, 1.0);
            e.setValue (0.0, juce::dontSendNotification);
        }
    }

    fltrKey.setLed (page == DotDisplay::Page::filter);
    trigKey.setLed (page == DotDisplay::Page::trig);
    display.refresh();
    repaint();
}

void MainPanel::timerCallback()
{
    display.refresh();

    const double now = juce::Time::getMillisecondCounterHiRes();
    const int tc = proc.live.trigCount.load();
    if (tc != lastTrigCount)
    {
        lastTrigCount = tc;
        trigLedMs = now;
    }

    const bool led = proc.live.gateOpen.load() || now - trigLedMs < 80.0;
    if (led != trigLed)
    {
        trigLed = led;
        repaint ((int) trigLedX - 30, 330, 90, 60);
    }
}

void MainPanel::resized()
{
    displayBezel = { 40, 62, 480, 258 };
    display.setBounds (displayBezel.reduced (16, 17));

    for (int i = 0; i < 8; ++i)
        encoders[(size_t) i].setBounds (knobCols[i % 4] - knobSize / 2, knobRows[i / 4] - knobSize / 2, knobSize, knobSize);

    fltrKey.setBounds (40, 336, 72, 62);
    trigKey.setBounds (122, 336, 72, 62);
}

void MainPanel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // Gehaeuse: mattschwarz eloxiert
    juce::ColourGradient housing (Hw::housingTop, 0.0f, 0.0f, Hw::housingBottom, 0.0f, b.getBottom(), false);
    g.setGradientFill (housing);
    g.fillRect (b);

    // Feine Kante oben (Lichtkante) und unten
    g.setColour (juce::Colour (0xff2e2e2f));
    g.drawHorizontalLine (0, 0.0f, b.getRight());
    g.setColour (juce::Colour (0xff070707));
    g.drawHorizontalLine ((int) b.getBottom() - 1, 0.0f, b.getRight());

    // Markenzug
    g.setColour (Hw::silkscreen);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold).withKerningFactor (0.18f));
    g.drawText ("RYTMFILTER", 40, 18, 220, 26, juce::Justification::centredLeft);
    g.setColour (Hw::silkDim);
    g.setFont (juce::FontOptions (11.0f).withKerningFactor (0.2f));
    g.drawText ("ANALOG-STYLE MULTIMODE FILTER", 232, 18, 300, 26, juce::Justification::centredLeft);

    // Display-Einfassung (vertieft)
    {
        const auto bz = displayBezel.toFloat();
        g.setColour (juce::Colour (0xff050505));
        g.fillRoundedRectangle (bz, 6.0f);
        g.setColour (juce::Colour (0xff2b2b2c));
        g.drawRoundedRectangle (bz.reduced (0.5f), 6.0f, 1.0f);
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.8f), bz.getX(), bz.getY(),
                                    juce::Colours::transparentBlack, bz.getX(), bz.getY() + 14.0f, false);
        g.setGradientFill (inner);
        g.fillRoundedRectangle (bz.reduced (1.0f).withHeight (14.0f), 5.0f);
    }

    // DATA ENTRY-Bereich
    g.setColour (Hw::silkDim);
    g.setFont (juce::FontOptions (11.0f).withKerningFactor (0.2f));
    g.drawText ("DATA ENTRY", knobCols[0] - 46, 62, 200, 16, juce::Justification::centredLeft);
    g.drawHorizontalLine (70, (float) knobCols[0] + 50.0f, (float) knobCols[3] + 46.0f);

    // Buchstaben A-H unter den Encodern
    g.setColour (Hw::silkscreen);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    for (int i = 0; i < 8; ++i)
        g.drawText (juce::String::charToString (knobLetters[i]), knobCols[i % 4] - 20, knobRows[i / 4] + 38, 40, 18,
                    juce::Justification::centred);

    // TRIG-LED neben den Tasten
    {
        const float lx = trigLedX, ly = trigLedY;
        g.setColour (Hw::silkscreen);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("TRIG IN", (int) lx - 20, 338, 60, 20, juce::Justification::centred);
        const auto led = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ lx + 10.0f, ly });
        if (trigLed)
        {
            g.setColour (Hw::ledRed.withAlpha (0.35f));
            g.fillEllipse (led.expanded (6.0f));
        }
        g.setColour (trigLed ? Hw::ledRed : juce::Colour (0xff2a0c0a));
        g.fillEllipse (led);
        g.setColour (juce::Colour (0xff050505));
        g.drawEllipse (led, 1.0f);
    }

    // Hinweis unten rechts
    g.setColour (Hw::silkDim);
    g.setFont (juce::FontOptions (11.0f).withKerningFactor (0.1f));
    g.drawText ("DRAG = EDIT   SHIFT = FINE   DOUBLE-CLICK = DEFAULT", 520, 380, 440, 18, juce::Justification::centredRight);
}
