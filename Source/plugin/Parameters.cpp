#include "Parameters.h"

namespace vv::params
{
namespace
{
    using Range = juce::NormalisableRange<float>;
    using Attributes = juce::AudioParameterFloatAttributes;

    Range skewed (float lo, float hi, float centre, float step = 0.0f)
    {
        Range r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }

    Attributes withFormat (const char* label, int decimals)
    {
        return Attributes()
            .withLabel (label)
            .withStringFromValueFunction ([label, decimals] (float v, int)
                                          { return juce::String (v, decimals) + " " + label; });
    }

    Attributes percent()
    {
        return Attributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; });
    }

    Attributes milliseconds()
    {
        return Attributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int)
                                          { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s"
                                                                : juce::String (v, v < 10.0f ? 1 : 0) + " ms"; });
    }

    void addFloat (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const char* pid, const char* name,
                   Range range, float def, Attributes attributes)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { pid, 1 }, name, range, def, attributes));
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const VocoderParams d; // defaults live in one place: the DSP struct

    // Modulator
    addFloat (layout, id::inGain, "Input Gain", Range (-24.0f, 24.0f, 0.1f), d.inputGainDb, withFormat ("dB", 1));
    addFloat (layout, id::gate, "Gate", Range (-90.0f, -20.0f, 0.5f), d.gateDb,
              Attributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                                                                        { return v <= -89.5f ? juce::String ("Off") : juce::String (v, 0) + " dB"; }));

    // Vocoder bank
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::bands, 1 }, "Bands",
                                                              juce::StringArray { "8", "10", "12", "16", "20", "24" }, 3));
    addFloat (layout, id::resonance, "Resonance", skewed (0.5f, 4.0f, 1.2f, 0.01f), d.resonance,
              Attributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + "x"; }));
    addFloat (layout, id::formant, "Formant", Range (-12.0f, 12.0f, 0.01f), d.formantSemitones, withFormat ("st", 1));
    addFloat (layout, id::attack, "Env Attack", skewed (0.5f, 100.0f, 10.0f, 0.1f), d.attackMs, milliseconds());
    addFloat (layout, id::release, "Env Release", skewed (5.0f, 1000.0f, 120.0f, 1.0f), d.releaseMs, milliseconds());
    addFloat (layout, id::brightness, "Brightness", Range (-6.0f, 12.0f, 0.1f), d.brightnessDbPerOct, withFormat ("dB/oct", 1));
    addFloat (layout, id::sibilance, "Sibilance", Range (0.0f, 1.0f, 0.01f), d.sibilance, percent());
    addFloat (layout, id::air, "Air", Range (0.0f, 1.0f, 0.01f), d.air, percent());

    // Carrier synth
    addFloat (layout, id::saw, "Saw", Range (0.0f, 1.0f, 0.01f), d.sawLevel, percent());
    addFloat (layout, id::pulse, "Pulse", Range (0.0f, 1.0f, 0.01f), d.pulseLevel, percent());
    addFloat (layout, id::pulseWidth, "Pulse Width", Range (0.05f, 0.5f, 0.01f), d.pulseWidth, percent());
    addFloat (layout, id::sub, "Sub", Range (0.0f, 1.0f, 0.01f), d.subLevel, percent());
    addFloat (layout, id::noise, "Noise", Range (0.0f, 1.0f, 0.01f), d.noiseLevel, percent());
    addFloat (layout, id::detune, "Detune", Range (0.0f, 50.0f, 0.1f), d.detuneCents, withFormat ("ct", 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::octave, 1 }, "Octave", -2, 2, d.octave));
    addFloat (layout, id::ampAttack, "Attack", skewed (1.0f, 3000.0f, 80.0f, 0.1f), d.ampAttackMs, milliseconds());
    addFloat (layout, id::ampRelease, "Release", skewed (5.0f, 5000.0f, 400.0f, 1.0f), d.ampReleaseMs, milliseconds());
    addFloat (layout, id::glide, "Glide", skewed (0.0f, 2000.0f, 150.0f, 1.0f), d.glideMs, milliseconds());
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::voiceMode, 1 }, "Voice Mode",
                                                              juce::StringArray { "Poly", "Mono" }, d.mono ? 1 : 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::bendRange, 1 }, "Bend Range", 0, 12,
                                                           static_cast<int> (d.bendRangeSemitones)));
    addFloat (layout, id::vibRate, "Vibrato Rate", skewed (0.1f, 12.0f, 4.0f, 0.01f), d.vibratoRateHz, withFormat ("Hz", 2));
    addFloat (layout, id::vibDepth, "Vibrato Depth", Range (0.0f, 1.0f, 0.01f), d.vibratoDepth, percent());

    // Output
    addFloat (layout, id::ensemble, "Ensemble", Range (0.0f, 1.0f, 0.01f), d.ensemble, percent());
    addFloat (layout, id::dry, "Dry Voice", Range (0.0f, 1.0f, 0.01f), d.dryVoice, percent());
    addFloat (layout, id::outGain, "Output", Range (-24.0f, 12.0f, 0.1f), d.outputGainDb, withFormat ("dB", 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::listen, 1 }, "Carrier Listen", d.carrierListen));

    return layout;
}

ParameterReader::ParameterReader (juce::AudioProcessorValueTreeState& state)
{
    auto bind = [&state] (Entry& e, const char* pid)
    {
        e.value = state.getRawParameterValue (pid);
        jassert (e.value != nullptr);
    };

    bind (inGain, id::inGain);
    bind (gate, id::gate);
    bind (bands, id::bands);
    bind (resonance, id::resonance);
    bind (formant, id::formant);
    bind (attack, id::attack);
    bind (release, id::release);
    bind (brightness, id::brightness);
    bind (sibilance, id::sibilance);
    bind (air, id::air);
    bind (saw, id::saw);
    bind (pulse, id::pulse);
    bind (pulseWidth, id::pulseWidth);
    bind (sub, id::sub);
    bind (noise, id::noise);
    bind (detune, id::detune);
    bind (octave, id::octave);
    bind (ampAttack, id::ampAttack);
    bind (ampRelease, id::ampRelease);
    bind (glide, id::glide);
    bind (voiceMode, id::voiceMode);
    bind (bendRange, id::bendRange);
    bind (vibRate, id::vibRate);
    bind (vibDepth, id::vibDepth);
    bind (ensemble, id::ensemble);
    bind (dry, id::dry);
    bind (outGain, id::outGain);
    bind (listen, id::listen);
}

VocoderParams ParameterReader::read() const
{
    VocoderParams p;
    p.inputGainDb = inGain.get();
    p.gateDb = gate.get();
    const int bandIndex = juce::jlimit (0, static_cast<int> (std::size (kBandCounts)) - 1, juce::roundToInt (bands.get()));
    p.numBands = kBandCounts[bandIndex];
    p.resonance = resonance.get();
    p.formantSemitones = formant.get();
    p.attackMs = attack.get();
    p.releaseMs = release.get();
    p.brightnessDbPerOct = brightness.get();
    p.sibilance = sibilance.get();
    p.air = air.get();
    p.sawLevel = saw.get();
    p.pulseLevel = pulse.get();
    p.pulseWidth = pulseWidth.get();
    p.subLevel = sub.get();
    p.noiseLevel = noise.get();
    p.detuneCents = detune.get();
    p.octave = juce::roundToInt (octave.get());
    p.ampAttackMs = ampAttack.get();
    p.ampReleaseMs = ampRelease.get();
    p.glideMs = glide.get();
    p.mono = voiceMode.get() > 0.5f;
    p.bendRangeSemitones = static_cast<float> (juce::roundToInt (bendRange.get()));
    p.vibratoRateHz = vibRate.get();
    p.vibratoDepth = vibDepth.get();
    p.ensemble = ensemble.get();
    p.dryVoice = dry.get();
    p.outputGainDb = outGain.get();
    p.carrierListen = listen.get() > 0.5f;
    return p;
}
} // namespace vv::params
