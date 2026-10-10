#pragma once

#include "CarrierSynth.h"
#include "Ensemble.h"
#include "VocoderBank.h"

#include <array>
#include <atomic>

namespace vv
{
// Plain values for every user parameter. The plugin wrapper fills this from
// its parameter tree once per block; tests fill it directly.
struct VocoderParams
{
    // Modulator (voice)
    float inputGainDb = 0.0f;
    float gateDb = -60.0f;

    // Vocoder bank
    int numBands = 16;
    float resonance = 1.0f;
    float formantSemitones = 0.0f;
    float attackMs = 4.0f;
    float releaseMs = 60.0f;
    float brightnessDbPerOct = 3.0f;
    float sibilance = 0.5f;
    float air = 0.25f;

    // Carrier synth
    float sawLevel = 1.0f;
    float pulseLevel = 0.0f;
    float pulseWidth = 0.3f;
    float subLevel = 0.35f;
    float noiseLevel = 0.0f;
    float detuneCents = 7.0f;
    int octave = 0;
    float ampAttackMs = 8.0f;
    float ampReleaseMs = 250.0f;
    float glideMs = 0.0f;
    bool mono = false;
    float bendRangeSemitones = 2.0f;
    float vibratoRateHz = 5.5f;
    float vibratoDepth = 0.0f;

    // Output
    float ensemble = 0.5f;
    float dryVoice = 0.0f;
    float outputGainDb = 0.0f;
    bool carrierListen = false;
};

struct MidiEvent
{
    int sampleOffset = 0;
    uint8_t status = 0, data1 = 0, data2 = 0;
};

class VocoderEngine
{
public:
    static constexpr int kChunkSize = 32;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setParams (const VocoderParams& p) { params = p; }

    // modulator: mono voice input (may be nullptr for silence).
    // events must be sorted by sampleOffset.
    void process (const float* modulator, float* outL, float* outR, int numSamples,
                  const MidiEvent* events, int numEvents);

    // Metering, safe to read from the UI thread.
    int getNumBands() const { return meterBandCount.load (std::memory_order_relaxed); }
    float getBandLevel (int band) const { return bandMeters[static_cast<size_t> (band)].load (std::memory_order_relaxed); }
    float getInputLevel() const { return inputMeter.load (std::memory_order_relaxed); }
    float getOutputLevel() const { return outputMeter.load (std::memory_order_relaxed); }
    uint32_t getMidiEventCount() const { return midiCounter.load (std::memory_order_relaxed); }
    int getActiveVoiceCount() const { return activeVoices.load (std::memory_order_relaxed); }

private:
    void applyParams();
    void renderChunk (const float* modulator, float* outL, float* outR, int numSamples);

    float sampleRate = 44100.0f;
    VocoderParams params;

    VocoderBank bank;
    CarrierSynth synth;
    Ensemble ensemble;
    Random noise { 0x9e3779b9u };

    SvfCoeffs rumbleCoeffs, sibilanceHpCoeffs, sibilanceLpCoeffs, airCoeffs;
    Svf rumbleFilter, sibilanceHp, sibilanceLp, airA, airB;
    EnvelopeFollower gateFollower, hfFollower, lfFollower;
    float gateGain = 0.0f, gateAttackCoef = 1.0f, gateReleaseCoef = 1.0f;
    float formantSemis = 0.0f, formantCoef = 1.0f;

    SmoothedValue inputGain, outputGain, sibilanceAmt, airAmt, dryAmt, ensembleAmt;

    float inputPeak = 0.0f, outputPeak = 0.0f;
    std::array<std::atomic<float>, VocoderBank::kMaxBands> bandMeters {};
    std::atomic<int> meterBandCount { 16 };
    std::atomic<float> inputMeter { 0.0f }, outputMeter { 0.0f };
    std::atomic<uint32_t> midiCounter { 0 };
    std::atomic<int> activeVoices { 0 };
};
} // namespace vv
