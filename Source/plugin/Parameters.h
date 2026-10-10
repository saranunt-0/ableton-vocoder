#pragma once

#include "dsp/VocoderEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace vv::params
{
// Parameter IDs are part of saved Live sets: never rename them.
namespace id
{
    inline constexpr const char* inGain = "inGain";
    inline constexpr const char* gate = "gate";
    inline constexpr const char* bands = "bands";
    inline constexpr const char* resonance = "resonance";
    inline constexpr const char* formant = "formant";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* brightness = "brightness";
    inline constexpr const char* sibilance = "sibilance";
    inline constexpr const char* air = "air";
    inline constexpr const char* saw = "saw";
    inline constexpr const char* pulse = "pulse";
    inline constexpr const char* pulseWidth = "pw";
    inline constexpr const char* sub = "sub";
    inline constexpr const char* noise = "noise";
    inline constexpr const char* detune = "detune";
    inline constexpr const char* octave = "octave";
    inline constexpr const char* ampAttack = "ampAttack";
    inline constexpr const char* ampRelease = "ampRelease";
    inline constexpr const char* glide = "glide";
    inline constexpr const char* voiceMode = "voiceMode";
    inline constexpr const char* bendRange = "bendRange";
    inline constexpr const char* vibRate = "vibRate";
    inline constexpr const char* vibDepth = "vibDepth";
    inline constexpr const char* ensemble = "ensemble";
    inline constexpr const char* dry = "dry";
    inline constexpr const char* outGain = "outGain";
    inline constexpr const char* listen = "listen";
} // namespace id

inline constexpr int kBandCounts[] = { 8, 10, 12, 16, 20, 24 };

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Caches the atomic parameter pointers so the audio thread never does string lookups.
class ParameterReader
{
public:
    explicit ParameterReader (juce::AudioProcessorValueTreeState& state);
    VocoderParams read() const;

private:
    struct Entry
    {
        std::atomic<float>* value = nullptr;
        float get() const { return value->load (std::memory_order_relaxed); }
    };

    Entry inGain, gate, bands, resonance, formant, attack, release, brightness, sibilance, air,
        saw, pulse, pulseWidth, sub, noise, detune, octave, ampAttack, ampRelease, glide, voiceMode,
        bendRange, vibRate, vibDepth, ensemble, dry, outGain, listen;
};
} // namespace vv::params
