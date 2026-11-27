// Rendert den Editor als PNG (ohne DAW), mit Beispielwerten und laufender Huellkurve.
// Aufruf: UiSnapshot <ausgabe.png> [skalierung] [fltr|trig] [typ 0..6]
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    const juce::File out = argc > 1 ? juce::File (juce::String (argv[1]))
                                    : juce::File::getCurrentWorkingDirectory().getChildFile ("snapshot.png");
    const double scale = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 1.0;
    const juce::String page = argc > 3 ? juce::String (argv[3]) : juce::String ("fltr");
    const int type = argc > 4 ? juce::String (argv[4]).getIntValue() : 0;

    RytmFilterProcessor proc;
    auto set = [&] (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    set (Params::atk, 6);  set (Params::dec, 70); set (Params::sus, 40); set (Params::rel, 60);
    set (Params::frq, 62); set (Params::res, 88); set (Params::typ, (float) type); set (Params::env, 32);

    const double sr = 48000.0;
    const int block = 512;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (juce::roundToInt (editor->getWidth() * scale), juce::roundToInt (editor->getHeight() * scale));

    // Kinder des Panels: 0 = Display, 1..8 = Encoder, 9 = FLTR, 10 = TRIG
    const int keyIndex = page == "trig" ? 10 : -1;
    if (keyIndex > 0)
        if (auto* key = dynamic_cast<juce::Button*> (editor->getChildComponent (0)->getChildComponent (keyIndex)))
            key->triggerClick();

    juce::AudioBuffer<float> buffer (2, block);
    juce::Random rng (1);

    for (int n = 0; n < (int) (sr * 0.6 / block); ++n)
    {
        for (int i = 0; i < block; ++i)
        {
            const float v = (rng.nextFloat() * 2.0f - 1.0f) * 0.3f;
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }
        juce::MidiBuffer midi;
        if (n == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
        proc.processBlock (buffer, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (12);
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    juce::PNGImageFormat png;
    out.deleteFile();
    juce::FileOutputStream stream (out);
    const bool ok = stream.openedOk() && png.writeImageToStream (image, stream);
    std::printf ("%s %s (%dx%d)\n", ok ? "geschrieben:" : "FEHLER:", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());

    editor.reset();
    return ok ? 0 : 1;
}
