#pragma once

#include "Parameters.h"
#include "dsp/VocoderEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#ifndef VV_INSTRUMENT
 #define VV_INSTRUMENT 1
#endif

class VintageVocoderProcessor : public juce::AudioProcessor
{
public:
    VintageVocoderProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return &vst3Extensions; }

    static constexpr bool isInstrument = VV_INSTRUMENT != 0;

    juce::AudioProcessorValueTreeState state;
    vv::VocoderEngine engine; // the editor reads its meters

private:
    // The instrument's only audio input must be reported to VST3 hosts as an
    // auxiliary (sidechain) bus, otherwise Ableton shows no Sidechain selector.
    struct Vst3Extensions : juce::VST3ClientExtensions
    {
        bool getPluginHasMainInput() const override { return ! isInstrument; }
    };

    static BusesProperties makeBuses();
    void renderSlice (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, int start, int length);

    Vst3Extensions vst3Extensions;
    vv::params::ParameterReader parameters;
    juce::AudioBuffer<float> modulator, scratchRight;
    std::vector<vv::MidiEvent> midiEvents, sliceEvents;
    int maxSliceSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VintageVocoderProcessor)
};
