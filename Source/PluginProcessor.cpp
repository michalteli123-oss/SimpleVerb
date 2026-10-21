#include "PluginProcessor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout SimpleVerbAudioProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using ID = juce::ParameterID;
    using R = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<F> (ID { "size", 1 },    "Room Size", R (0.0f, 1.0f, 0.001f), 0.5f));
    p.push_back (std::make_unique<F> (ID { "damping", 1 }, "Damping",   R (0.0f, 1.0f, 0.001f), 0.5f));
    p.push_back (std::make_unique<F> (ID { "width", 1 },   "Width",     R (0.0f, 1.0f, 0.001f), 1.0f));
    p.push_back (std::make_unique<F> (ID { "predelay", 1 },"Pre-delay (ms)", R (0.0f, 200.0f, 0.1f), 20.0f));
    p.push_back (std::make_unique<F> (ID { "lowcut", 1 },  "Low Cut (Hz)",   R (20.0f, 1000.0f, 1.0f, 0.4f), 100.0f));
    p.push_back (std::make_unique<F> (ID { "highcut", 1 }, "High Cut (Hz)",  R (1000.0f, 20000.0f, 1.0f, 0.4f), 12000.0f));
    p.push_back (std::make_unique<F> (ID { "mix", 1 },     "Mix",       R (0.0f, 1.0f, 0.001f), 0.3f));
    p.push_back (std::make_unique<juce::AudioParameterBool> (ID { "freeze", 1 }, "Freeze", false));

    return { p.begin(), p.end() };
}

SimpleVerbAudioProcessor::SimpleVerbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pSize     = apvts.getRawParameterValue ("size");
    pDamp     = apvts.getRawParameterValue ("damping");
    pWidth    = apvts.getRawParameterValue ("width");
    pPreDelay = apvts.getRawParameterValue ("predelay");
    pLowCut   = apvts.getRawParameterValue ("lowcut");
    pHighCut  = apvts.getRawParameterValue ("highcut");
    pMix      = apvts.getRawParameterValue ("mix");
    pFreeze   = apvts.getRawParameterValue ("freeze");
}

bool SimpleVerbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SimpleVerbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) samplesPerBlock,
                                  (juce::uint32) getTotalNumOutputChannels() };

    preDelay.setMaximumDelayInSamples ((int) (sampleRate * 0.25) + 1);
    preDelay.prepare (spec);
    preDelay.reset();

    lowCut.prepare (spec);
    lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lowCut.reset();

    highCut.prepare (spec);
    highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    highCut.reset();

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    dryBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock);

    lastWetGain = lastDryGain = -1.0f; // wymusza ustawienie przy pierwszym bloku
}

void SimpleVerbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // kopia sygnału suchego
    dryBuffer.setSize (numCh, n, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

    // pre-delay + filtry (tylko tor wet)
    preDelay.setDelay ((float) (*pPreDelay * 0.001 * currentSampleRate));
    lowCut.setCutoffFrequency (*pLowCut);
    highCut.setCutoffFrequency (*pHighCut);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    preDelay.process (ctx);
    lowCut.process (ctx);
    highCut.process (ctx);

    // reverb (100% wet)
    juce::Reverb::Parameters rp;
    rp.roomSize   = *pSize;
    rp.damping    = *pDamp;
    rp.width      = *pWidth;
    rp.wetLevel   = 1.0f;
    rp.dryLevel   = 0.0f;
    rp.freezeMode = *pFreeze > 0.5f ? 1.0f : 0.0f;
    reverb.setParameters (rp);

    if (numCh >= 2)
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    else
        reverb.processMono (buffer.getWritePointer (0), n);

    // miks (equal-power) z płynną zmianą gainu
    const float mix = *pMix;
    const float wetGain = std::sin (mix * juce::MathConstants<float>::halfPi);
    const float dryGain = std::cos (mix * juce::MathConstants<float>::halfPi);

    if (lastWetGain < 0.0f) { lastWetGain = wetGain; lastDryGain = dryGain; }

    buffer.applyGainRamp (0, n, lastWetGain, wetGain);
    for (int ch = 0; ch < numCh; ++ch)
        buffer.addFromWithRamp (ch, 0, dryBuffer.getReadPointer (ch), n, lastDryGain, dryGain);

    lastWetGain = wetGain;
    lastDryGain = dryGain;
}

juce::AudioProcessorEditor* SimpleVerbAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void SimpleVerbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SimpleVerbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleVerbAudioProcessor();
}
