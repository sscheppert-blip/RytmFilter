#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "DotDisplay.h"
#include "HardwareLookAndFeel.h"

// Das "Geraet": Gehaeuse, Display, 8 Encoder (A-H), Seitentasten. Feste Basisgroesse,
// wird vom Editor per Transform skaliert.
class MainPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int baseWidth  = 1000;
    static constexpr int baseHeight = 420;

    explicit MainPanel (RytmFilterProcessor&);
    ~MainPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setPage (DotDisplay::Page);

    RytmFilterProcessor& proc;
    HardwareLookAndFeel lnf;

    DotDisplay display;
    std::array<juce::Slider, 8> encoders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> attachments;

    HardwareKey fltrKey { "FLTR" }, trigKey { "TRIG" };

    juce::Rectangle<int> displayBezel;
    int lastTrigCount = 0;
    double trigLedMs = 0.0;
    bool trigLed = false;
};
