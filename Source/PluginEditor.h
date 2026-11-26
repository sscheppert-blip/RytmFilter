#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MainPanel.h"

// Frei skalierbarer Rahmen: MainPanel wird in Basisgroesse gelayoutet und per AffineTransform
// skaliert. JUCE rendert alle Pfade/Schriften nativ in Zielgroesse -> scharf wie SVG.
class RytmFilterEditor : public juce::AudioProcessorEditor
{
public:
    explicit RytmFilterEditor (RytmFilterProcessor&);
    ~RytmFilterEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RytmFilterProcessor& proc;
    MainPanel panel;
    bool constructed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RytmFilterEditor)
};
