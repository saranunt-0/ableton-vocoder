#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr size_t kMaxMidiEventsPerBlock = 4096;
} // namespace

juce::AudioProcessor::BusesProperties VintageVocoderProcessor::makeBuses()
{
    const auto stereo = juce::AudioChannelSet::stereo();
    if constexpr (isInstrument)
        return BusesProperties()
            .withInput ("Voice (Sidechain)", stereo, true)
            .withOutput ("Output", stereo, true);
    else
        return BusesProperties()
            .withInput ("Voice", stereo, true)
            .withOutput ("Output", stereo, true);
}

VintageVocoderProcessor::VintageVocoderProcessor()
    : AudioProcessor (makeBuses()),
      state (*this, nullptr, "VintageVocoder", vv::params::createLayout()),
      parameters (state)
{
    midiEvents.reserve (kMaxMidiEventsPerBlock);
    sliceEvents.reserve (kMaxMidiEventsPerBlock);
}

void VintageVocoderProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxSliceSize = juce::jmax (32, samplesPerBlock);
    modulator.setSize (1, maxSliceSize);
    scratchRight.setSize (1, maxSliceSize);
    engine.setParams (parameters.read());
    engine.prepare (sampleRate, maxSliceSize);
}

bool VintageVocoderProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    for (const auto& in : layouts.inputBuses)
        if (! in.isDisabled() && in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
            return false;

    return true;
}

void VintageVocoderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    midiEvents.clear();
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.getRawDataSize() > 3 || msg.isSysEx() || midiEvents.size() >= kMaxMidiEventsPerBlock)
            continue;
        const auto* raw = msg.getRawData();
        midiEvents.push_back ({ meta.samplePosition, raw[0],
                                static_cast<uint8_t> (msg.getRawDataSize() > 1 ? raw[1] : 0),
                                static_cast<uint8_t> (msg.getRawDataSize() > 2 ? raw[2] : 0) });
    }

    engine.setParams (parameters.read());

    const auto input = getBusBuffer (buffer, true, 0);
    auto output = getBusBuffer (buffer, false, 0);
    const int numSamples = buffer.getNumSamples();

    // Hosts may exceed the announced block size; slice instead of allocating.
    for (int start = 0; start < numSamples; start += maxSliceSize)
        renderSlice (input, output, start, juce::jmin (maxSliceSize, numSamples - start));

    for (int ch = getTotalNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

void VintageVocoderProcessor::renderSlice (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output,
                                           int start, int length)
{
    // Sum the voice to mono first: input and output share channel memory.
    auto* mod = modulator.getWritePointer (0);
    juce::FloatVectorOperations::clear (mod, length);
    const int inChannels = input.getNumChannels();
    for (int ch = 0; ch < inChannels; ++ch)
        juce::FloatVectorOperations::addWithMultiply (mod, input.getReadPointer (ch, start), 1.0f / static_cast<float> (inChannels), length);

    sliceEvents.clear();
    for (const auto& e : midiEvents)
        if (e.sampleOffset >= start && e.sampleOffset < start + length)
            sliceEvents.push_back ({ e.sampleOffset - start, e.status, e.data1, e.data2 });

    if (output.getNumChannels() == 0)
        return;

    float* left = output.getWritePointer (0, start);
    float* right = output.getNumChannels() > 1 ? output.getWritePointer (1, start) : scratchRight.getWritePointer (0);

    engine.process (mod, left, right, length, sliceEvents.data(), static_cast<int> (sliceEvents.size()));

    if (output.getNumChannels() == 1)
        for (int i = 0; i < length; ++i)
            left[i] = 0.5f * (left[i] + right[i]);
}

juce::AudioProcessorEditor* VintageVocoderProcessor::createEditor()
{
    return new VintageVocoderEditor (*this);
}

void VintageVocoderProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VintageVocoderProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VintageVocoderProcessor();
}
