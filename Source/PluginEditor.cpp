#include "PluginEditor.h"

namespace
{
    const juce::Identifier uiScaleId { "uiScale" };
    constexpr double minScale = 0.6, maxScale = 2.5;
}

RytmFilterEditor::RytmFilterEditor (RytmFilterProcessor& p)
    : AudioProcessorEditor (&p), proc (p), panel (p)
{
    addAndMakeVisible (panel);

    constexpr int w = MainPanel::baseWidth, h = MainPanel::baseHeight;

    // vor setResizeLimits lesen: das ruft resized() bereits mit Minimalgroesse auf
    const double scale = juce::jlimit (minScale, maxScale, (double) proc.apvts.state.getProperty (uiScaleId, 1.0));

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (w * minScale), juce::roundToInt (h * minScale),
                     juce::roundToInt (w * maxScale), juce::roundToInt (h * maxScale));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) w / (double) h);

    setSize (juce::roundToInt (w * scale), juce::roundToInt (h * scale));
    constructed = true;
}

void RytmFilterEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121213));
}

void RytmFilterEditor::resized()
{
    const float scale = juce::jmin ((float) getWidth()  / (float) MainPanel::baseWidth,
                                    (float) getHeight() / (float) MainPanel::baseHeight);

    panel.setBounds (0, 0, MainPanel::baseWidth, MainPanel::baseHeight);
    panel.setTransform (juce::AffineTransform::scale (scale));

    if (constructed)
        proc.apvts.state.setProperty (uiScaleId, scale, nullptr);
}
