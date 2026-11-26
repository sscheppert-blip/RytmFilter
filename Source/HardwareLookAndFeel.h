#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Hw
{
    const juce::Colour housingTop    { 0xff1c1c1d };
    const juce::Colour housingBottom { 0xff121213 };
    const juce::Colour silkscreen    { 0xffd6d6d6 };
    const juce::Colour silkDim       { 0xff8c8c8c };
    const juce::Colour ledRed        { 0xffff3b2f };
    const juce::Colour ledWhite      { 0xfff2f2f2 };
}

// Mattschwarze Endlos-Encoder (Draufsicht): Riffelrand, flache Oberseite, weisser Strich.
class HardwareLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto area   = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float r     = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 4.0f;
        const auto c      = area.getCentre();
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const bool enabled = slider.isEnabled();

        // Schlagschatten
        {
            juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.65f), c.x, c.y + r * 0.15f,
                                         juce::Colours::transparentBlack, c.x, c.y + r * 1.35f, true);
            g.setGradientFill (shadow);
            g.fillEllipse (juce::Rectangle<float> (r * 2.7f, r * 2.7f).withCentre (c.translated (0.0f, r * 0.12f)));
        }

        // Aussenring (Seite) mit Riffelung
        g.setColour (juce::Colour (0xff0b0b0b));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));

        const int ridges = 36;
        for (int i = 0; i < ridges; ++i)
        {
            const float a = angle + juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
            const auto p1 = c.getPointOnCircumference (r * 0.86f, a);
            const auto p2 = c.getPointOnCircumference (r * 0.99f, a);
            // Licht von oben links: Rippen oben heller
            const float light = 0.5f + 0.5f * std::cos (a + 0.8f);
            g.setColour (juce::Colour (0xff1a1a1a).interpolatedWith (juce::Colour (0xff3a3a3a), light * 0.6f));
            g.drawLine ({ p1, p2 }, juce::jmax (1.0f, r * 0.045f));
        }

        // Oberseite: matt, leichter Verlauf
        const float top = r * 0.80f;
        juce::ColourGradient face (juce::Colour (0xff2a2a2b), c.x - top * 0.6f, c.y - top * 0.8f,
                                   juce::Colour (0xff121213), c.x + top * 0.5f, c.y + top, false);
        g.setGradientFill (face);
        g.fillEllipse (juce::Rectangle<float> (top * 2.0f, top * 2.0f).withCentre (c));

        // Kante der Oberseite
        g.setColour (juce::Colour (0xff3c3c3d).withAlpha (0.9f));
        g.drawEllipse (juce::Rectangle<float> (top * 2.0f, top * 2.0f).withCentre (c), juce::jmax (0.8f, r * 0.02f));

        // Weisser Markierungsstrich
        juce::Path line;
        line.startNewSubPath (c.getPointOnCircumference (top * 0.30f, angle));
        line.lineTo (c.getPointOnCircumference (top * 0.88f, angle));
        g.setColour (enabled ? juce::Colour (0xffededed) : juce::Colour (0xff4a4a4a));
        g.strokePath (line, juce::PathStrokeType (juce::jmax (1.5f, r * 0.075f), juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }
};

// Gummitaste mit LED, Beschriftung im Siebdruck ueber der Taste.
class HardwareKey : public juce::Button
{
public:
    explicit HardwareKey (const juce::String& label) : juce::Button (label) {}

    void setLed (bool on, juce::Colour c = Hw::ledWhite) { ledOn = on; ledColour = c; repaint(); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat();

        // Siebdruck-Beschriftung oben
        auto labelArea = b.removeFromTop (b.getHeight() * 0.32f);
        g.setColour (Hw::silkscreen);
        g.setFont (juce::FontOptions (labelArea.getHeight() * 0.72f, juce::Font::bold));
        g.drawText (getButtonText(), labelArea, juce::Justification::centred);

        auto key = b.reduced (b.getWidth() * 0.08f, b.getHeight() * 0.08f);
        if (down) key = key.translated (0.0f, 1.0f);
        const float corner = key.getHeight() * 0.22f;

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (key.translated (0.0f, 2.0f).expanded (1.0f), corner);

        juce::ColourGradient grad (juce::Colour (over ? 0xff363637 : 0xff2e2e2f), key.getX(), key.getY(),
                                   juce::Colour (0xff1d1d1e), key.getX(), key.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (key, corner);
        g.setColour (juce::Colour (0xff444445));
        g.drawRoundedRectangle (key.reduced (0.5f), corner, 1.0f);

        // LED
        const auto led = juce::Rectangle<float> (key.getWidth() * 0.34f, key.getHeight() * 0.14f)
                             .withCentre ({ key.getCentreX(), key.getY() + key.getHeight() * 0.3f });
        if (ledOn)
        {
            g.setColour (ledColour.withAlpha (0.35f));
            g.fillRoundedRectangle (led.expanded (led.getHeight() * 0.8f), led.getHeight());
        }
        g.setColour (ledOn ? ledColour : juce::Colour (0xff141414));
        g.fillRoundedRectangle (led, led.getHeight() * 0.5f);
    }

private:
    bool ledOn = false;
    juce::Colour ledColour = Hw::ledWhite;
};
