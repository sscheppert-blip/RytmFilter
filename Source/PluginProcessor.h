#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "FilterDSP.h"
#include "Params.h"

class RytmFilterProcessor : public juce::AudioProcessor
{
public:
    RytmFilterProcessor();
    ~RytmFilterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Live-Werte fuer das Display (Audio-Thread schreibt, UI liest)
    struct LiveState
    {
        std::atomic<float> cutoffHz { 20000.0f };   // wo der Filter gerade greift (inkl. Huellkurve)
        std::atomic<float> envLevel { 0.0f };
        std::atomic<bool>  gateOpen { false };
        std::atomic<int>   trigCount { 0 };
        std::atomic<float> bpm { 120.0f };
        std::atomic<bool>  playing { false };
        std::atomic<double> sampleRate { 44100.0 };
    } live;

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int num,
                       juce::MidiBufferIterator& midiIt, const juce::MidiBufferIterator& midiEnd,
                       double ppqStart, double ppqPerSample, bool isPlaying);
    void trigger();

    struct RawParams
    {
        std::atomic<float>* atk; std::atomic<float>* dec; std::atomic<float>* sus; std::atomic<float>* rel;
        std::atomic<float>* frq; std::atomic<float>* res; std::atomic<float>* typ; std::atomic<float>* env;
        std::atomic<float>* mode; std::atomic<float>* rate; std::atomic<float>* sens; std::atomic<float>* len;
    } p {};

    // Version der FRQ-Kennlinie im gespeicherten Zustand (1 = v1.0: 20 Hz..20 kHz, 2 = Halbtonschritte)
    static constexpr int frqCurveVersion = 2;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::array<dsp::MultimodeSvf, 2> filters;
    std::vector<dsp::SvfCoeffs> coeffs;
    dsp::Envelope envelope;
    dsp::TransientDetector detector;

    juce::SmoothedValue<double> frqSmoothed, resSmoothed;

    int chunkSize = 512;
    double baseRate = 44100.0;
    int heldNotes = 0;
    int gateSamplesLeft = 0;
    long long lastSyncStep = -1;
    float lastCutoffHz = 20000.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RytmFilterProcessor)
};
