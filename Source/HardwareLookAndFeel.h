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

// Schlanke Endlos-Encoder (Draufsicht) im Stil der Elektron-Geraete: glatt, mattschwarz
// gummiert, flache Kappe, KEINE Positionsmarke - ein Endlos-Encoder hat keinen Anschlag,
// der Wert steht im Display.
class HardwareLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Anteil des Komponenten-Rechtecks, den der Knopf einnimmt (schlanker als ein Poti-Knopf)
    static constexpr float knobDiameter = 0.64f;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float /*sliderPos*/,
                           float /*startAngle*/, float /*endAngle*/, juce::Slider& slider) override
    {
        const auto area  = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float r    = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f * knobDiameter;
        const auto c     = area.getCentre();
        const bool active = slider.isEnabled();
        auto circle = [&] (float radius, juce::Point<float> centre) { return juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre); };

        // Schlagschatten (Licht von oben links)
        {
            juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.7f), c.x, c.y + r * 0.2f,
                                         juce::Colours::transparentBlack, c.x, c.y + r * 1.6f, true);
            g.setGradientFill (shadow);
            g.fillEllipse (circle (r * 1.5f, c.translated (r * 0.08f, r * 0.18f)));
        }

        // Montagering im Gehaeuse
        g.setColour (juce::Colour (0xff070707));
        g.fillEllipse (circle (r * 1.08f, c));

        // Glatte Flanke: Zylinder von leicht schraeg oben -> oben links Licht, unten rechts Schatten
        {
            juce::ColourGradient side (juce::Colour (0xff3a3a3b), c.x - r * 0.7f, c.y - r * 0.7f,
                                       juce::Colour (0xff0a0a0a), c.x + r * 0.6f, c.y + r * 0.7f, false);
            side.addColour (0.45, juce::Colour (0xff1c1c1d));
            g.setGradientFill (side);
            g.fillEllipse (circle (r, c));
        }

        // Kappe: flach, matt gummiert, minimal versetzt (Perspektive)
        const float top = r * 0.84f;
        const auto capC = c.translated (-r * 0.02f, -r * 0.03f);
        const auto cap  = circle (top, capC);
        juce::ColourGradient face (juce::Colour (active ? 0xff29292a : 0xff1e1e1f), capC.x - top * 0.7f, capC.y - top * 0.8f,
                                   juce::Colour (0xff131314), capC.x + top * 0.6f, capC.y + top, false);
        g.setGradientFill (face);
        g.fillEllipse (cap);

        // Matter, breiter Glanz (Soft-Touch, kein Spiegeln)
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (active ? 0.055f : 0.025f), capC.x - top * 0.3f, capC.y - top * 0.4f,
                                    juce::Colours::transparentWhite, capC.x - top * 0.3f + top * 1.1f, capC.y - top * 0.4f, true);
        g.setGradientFill (sheen);
        g.fillEllipse (cap);

        // Abgerundete Kappenkante: oben links Lichtkante, unten rechts dunkel
        juce::ColourGradient rim (juce::Colour (0xff4a4a4b), capC.x - top, capC.y - top,
                                  juce::Colour (0xff0c0c0c), capC.x + top, capC.y + top, false);
        g.setGradientFill (rim);
        g.drawEllipse (cap, juce::jmax (0.7f, r * 0.03f));
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
