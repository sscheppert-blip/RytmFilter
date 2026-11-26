#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int osFactorLog2 = 1; // 2x Oversampling
}

RytmFilterProcessor::RytmFilterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RytmFilterState", createParameterLayout())
{
    p.atk  = apvts.getRawParameterValue (Params::atk);
    p.dec  = apvts.getRawParameterValue (Params::dec);
    p.sus  = apvts.getRawParameterValue (Params::sus);
    p.rel  = apvts.getRawParameterValue (Params::rel);
    p.frq  = apvts.getRawParameterValue (Params::frq);
    p.res  = apvts.getRawParameterValue (Params::res);
    p.typ  = apvts.getRawParameterValue (Params::typ);
    p.env  = apvts.getRawParameterValue (Params::env);
    p.mode = apvts.getRawParameterValue (Params::trigMode);
    p.rate = apvts.getRawParameterValue (Params::trigRate);
    p.sens = apvts.getRawParameterValue (Params::trigSens);
    p.len  = apvts.getRawParameterValue (Params::trigLen);
}

juce::AudioProcessorValueTreeState::ParameterLayout RytmFilterProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto intText = [] (float v, int) { return String (roundToInt (v)); };
    auto add127 = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 127.0f), def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (intText)));
    };

    add127 (Params::atk, "Filter Attack", 0.0f);
    add127 (Params::dec, "Filter Decay", 64.0f);
    add127 (Params::sus, "Filter Sustain", 0.0f);
    add127 (Params::rel, "Filter Release", 64.0f);

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { Params::frq, 1 }, "Filter Frequency", NormalisableRange<float> (0.0f, 127.0f), 127.0f,
                    AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " (" + String (Params::frqToHz ((float) roundToInt (v)), 0) + " Hz)"; })));

    add127 (Params::res, "Filter Resonance", 0.0f);

    StringArray types;
    for (int t = 0; t < Params::numTypes; ++t)
        types.add (Params::typeLong (t));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { Params::typ, 1 }, "Filter Type", types, Params::LP2));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { Params::env, 1 }, "Filter Env Depth", NormalisableRange<float> (-64.0f, 63.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (intText)));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { Params::trigMode, 1 }, "Trig Mode",
                                                        StringArray { "MIDI", "Transient", "Sync" }, Params::MIDI));
    StringArray rates;
    for (int r = 0; r < Params::numRates; ++r)
        rates.add (Params::rateShort (r));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { Params::trigRate, 1 }, "Trig Rate", rates, 1));
    add127 (Params::trigSens, "Trig Sensitivity", 64.0f);
    add127 (Params::trigLen, "Trig Length", 48.0f);

    return layout;
}

bool RytmFilterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void RytmFilterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseRate  = sampleRate;
    chunkSize = juce::jmax (32, samplesPerBlock);

    const int numCh = juce::jmax (1, getTotalNumOutputChannels());
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numCh, osFactorLog2,
                       juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampling->initProcessing ((size_t) chunkSize);
    setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples()));

    coeffs.assign ((size_t) chunkSize, {});
    for (auto& f : filters)
        f.reset();

    envelope.sampleRate = sampleRate;
    envelope.reset();
    detector.sampleRate = sampleRate;
    detector.reset();

    frqSmoothed.reset (sampleRate, 0.02);
    resSmoothed.reset (sampleRate, 0.02);
    frqSmoothed.setCurrentAndTargetValue (p.frq->load());
    resSmoothed.setCurrentAndTargetValue (p.res->load());

    heldNotes = 0;
    gateSamplesLeft = 0;
    lastSyncStep = -1;
    live.sampleRate.store (sampleRate);
}

void RytmFilterProcessor::trigger()
{
    envelope.gateOn();
    gateSamplesLeft = juce::jmax (1, (int) (Params::gateLenMs (p.len->load()) * 0.001 * baseRate));
    live.trigCount.fetch_add (1);
}

void RytmFilterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Tempo / Position fuer SYNC
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())          bpm = *b;
            if (auto q = pos->getPpqPosition())  ppq = *q;
            playing = pos->getIsPlaying();
        }
    live.bpm.store ((float) bpm);
    live.playing.store (playing);

    const double ppqPerSample = bpm / (60.0 * baseRate);

    auto midiIt  = midi.cbegin();
    const auto midiEnd = midi.cend();

    for (int start = 0; start < buffer.getNumSamples(); start += chunkSize)
    {
        const int num = juce::jmin (chunkSize, buffer.getNumSamples() - start);
        processChunk (buffer, start, num, midiIt, midiEnd, ppq + start * ppqPerSample, ppqPerSample, playing);
    }

    live.cutoffHz.store (lastCutoffHz);
    live.envLevel.store ((float) envelope.level);
    live.gateOpen.store (heldNotes > 0 || gateSamplesLeft > 0);
}

void RytmFilterProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int num,
                                        juce::MidiBufferIterator& midiIt, const juce::MidiBufferIterator& midiEnd,
                                        double ppqStart, double ppqPerSample, bool isPlaying)
{
    const int numCh = juce::jmin (buffer.getNumChannels(), (int) filters.size());
    const int mode  = juce::roundToInt (p.mode->load());
    const int type  = juce::roundToInt (p.typ->load());
    const double depth = p.env->load() * (127.0 / 63.5); // ENV +63 ~ voller Regelbereich
    const double atk = p.atk->load(), dec = p.dec->load(), sus = p.sus->load(), rel = p.rel->load();
    const double sens = p.sens->load();
    const double step = Params::rateInQuarters (juce::roundToInt (p.rate->load()));
    const double osRate = baseRate * (1 << osFactorLog2);

    frqSmoothed.setTargetValue (p.frq->load());
    resSmoothed.setTargetValue (p.res->load());

    if (! isPlaying)
        lastSyncStep = -1;

    // --- Steuerrate (Basis-Samplerate): Trigger, Huellkurve, Koeffizienten ---
    for (int i = 0; i < num; ++i)
    {
        const int pos = start + i;

        // MIDI immer abarbeiten (Iterator muss weiterlaufen), aber nur im MIDI-Modus auswerten
        while (midiIt != midiEnd && (*midiIt).samplePosition <= pos)
        {
            const auto msg = (*midiIt).getMessage();
            if (mode == Params::MIDI)
            {
                if (msg.isNoteOn())
                {
                    ++heldNotes;
                    envelope.gateOn();
                    live.trigCount.fetch_add (1);
                }
                else if (msg.isNoteOff())
                {
                    heldNotes = juce::jmax (0, heldNotes - 1);
                    if (heldNotes == 0)
                        envelope.gateOff();
                }
                else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                {
                    heldNotes = 0;
                    envelope.gateOff();
                }
            }
            ++midiIt;
        }

        if (mode == Params::TRANSIENT)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                mono += buffer.getSample (ch, pos);
            if (detector.process (mono / (float) juce::jmax (1, numCh), sens))
                trigger();
        }
        else if (mode == Params::SYNC && isPlaying)
        {
            const double q = ppqStart + i * ppqPerSample;
            const auto stepIndex = (long long) std::floor (q / step + 1.0e-9);
            if (stepIndex != lastSyncStep)
            {
                lastSyncStep = stepIndex;
                trigger();
            }
        }

        if (mode != Params::MIDI && gateSamplesLeft > 0 && --gateSamplesLeft == 0)
            envelope.gateOff();

        const double e   = envelope.process (atk, dec, sus, rel);
        const double frq = juce::jlimit (0.0, 127.0, frqSmoothed.getNextValue() + e * depth);
        const double hz  = Params::frqToHz (frq);
        coeffs[(size_t) i].set (hz, resSmoothed.getNextValue(), type, osRate);
        lastCutoffHz = (float) hz;
    }

    // --- Audio: 2x Oversampling, Filter pro Kanal ---
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) start, (size_t) num);
    auto up = oversampling->processSamplesUp (block);

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = up.getChannelPointer ((size_t) ch);
        auto& f = filters[(size_t) ch];
        for (size_t j = 0; j < up.getNumSamples(); ++j)
            d[j] = f.process (d[j], coeffs[j >> osFactorLog2]);
    }

    oversampling->processSamplesDown (block);
}

juce::AudioProcessorEditor* RytmFilterProcessor::createEditor()
{
    return new RytmFilterEditor (*this);
}

void RytmFilterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RytmFilterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RytmFilterProcessor();
}
