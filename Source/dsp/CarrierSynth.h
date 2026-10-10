#pragma once

#include "DspCommon.h"

#include <array>

namespace vv
{
// The keyboard half of the vocoder: a polyphonic divide-down-style oscillator
// bank (saw / pulse / sub / noise) with a simple AR envelope, glide, bend,
// vibrato and sustain pedal. Output is mono; stereo comes from the ensemble.
class CarrierSynth
{
public:
    static constexpr int kMaxVoices = 12;

    struct Settings
    {
        float sawLevel = 1.0f;
        float pulseLevel = 0.0f;
        float pulseWidth = 0.3f;     // 0.05 .. 0.5
        float subLevel = 0.35f;
        float noiseLevel = 0.0f;
        float detuneCents = 7.0f;
        int octave = 0;
        float attackSeconds = 0.008f;
        float releaseSeconds = 0.25f;
        float glideSeconds = 0.0f;
        bool mono = false;
        float bendRangeSemitones = 2.0f;
        float vibratoRateHz = 5.5f;
        float vibratoDepth = 0.0f;   // 0..1, mod wheel adds on top
    };

    void prepare (float newSampleRate)
    {
        sampleRate = newSampleRate;
        activitySmoother.prepare (sampleRate, 0.03f);
        reset();
    }

    void reset()
    {
        for (auto& v : voices)
            v = Voice {};
        heldCount = 0;
        sustainPedal = false;
        pitchBend = 0.0f;
        modWheel = 0.0f;
        lastNotePitch = 60.0f;
        activitySmoother.snap (0.0f);
    }

    void setSettings (const Settings& s) { settings = s; }

    void handleMidi (uint8_t status, uint8_t data1, uint8_t data2)
    {
        const uint8_t type = status & 0xF0;

        if (type == 0x90 && data2 > 0)
            noteOn (data1);
        else if (type == 0x80 || (type == 0x90 && data2 == 0))
            noteOff (data1);
        else if (type == 0xE0)
            pitchBend = (static_cast<float> ((data2 << 7) | data1) - 8192.0f) / 8192.0f;
        else if (type == 0xB0)
        {
            if (data1 == 1)
                modWheel = static_cast<float> (data2) / 127.0f;
            else if (data1 == 64)
                setSustain (data2 >= 64);
            else if (data1 == 120)
                reset(); // all sound off
            else if (data1 == 123)
                allNotesOff();
        }
    }

    // Updates pitch-related state once per chunk; frequency is constant within a chunk.
    void beginChunk (int numSamples)
    {
        const float chunk = static_cast<float> (numSamples);
        vibratoPhase += settings.vibratoRateHz * chunk / sampleRate;
        vibratoPhase -= std::floor (vibratoPhase);

        const float depth = std::clamp (settings.vibratoDepth + modWheel, 0.0f, 1.0f);
        const float vibrato = 0.5f * depth * std::sin (kTwoPi * vibratoPhase); // up to +-50 cents
        const float offset = 12.0f * static_cast<float> (settings.octave) + pitchBend * settings.bendRangeSemitones + vibrato;

        const float glideCoef = settings.glideSeconds > 0.0f
                                  ? 1.0f - std::exp (-chunk / (settings.glideSeconds * sampleRate))
                                  : 1.0f;
        const float detuneRatio = std::pow (2.0f, settings.detuneCents / 1200.0f);

        attackStep = 1.0f / std::max (1.0f, settings.attackSeconds * sampleRate);
        releaseCoef = std::exp (std::log (0.001f) / std::max (1.0f, settings.releaseSeconds * sampleRate));
        detuneMix = std::clamp (settings.detuneCents / 2.0f, 0.0f, 1.0f);

        for (auto& v : voices)
        {
            if (! v.active)
                continue;
            v.pitch += glideCoef * (v.targetPitch - v.pitch);
            const float freq = 440.0f * std::pow (2.0f, (v.pitch + offset - 69.0f) / 12.0f);
            v.inc = std::min (freq / sampleRate, 0.45f);
            v.inc2 = std::min (v.inc * detuneRatio, 0.45f);
        }
    }

    inline float renderSample()
    {
        float out = 0.0f;
        float envSum = 0.0f;
        const float pw = std::clamp (settings.pulseWidth, 0.05f, 0.5f);

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            if (v.releasing)
            {
                v.env *= releaseCoef;
                if (v.env < 1.0e-4f)
                {
                    v.active = false;
                    continue;
                }
            }
            else
                v.env = std::min (1.0f, v.env + attackStep);

            const float saw1 = 2.0f * v.phase - 1.0f - polyBlep (v.phase, v.inc);
            const float saw2 = 2.0f * v.phase2 - 1.0f - polyBlep (v.phase2, v.inc2);
            const float saw = (saw1 + detuneMix * saw2) / (1.0f + detuneMix);

            float pulse = v.phase < pw ? 1.0f : -1.0f;
            pulse += polyBlep (v.phase, v.inc);
            pulse -= polyBlep (wrap (v.phase + 1.0f - pw), v.inc);

            const float subInc = 0.5f * v.inc;
            float sub = v.subPhase < 0.5f ? 1.0f : -1.0f;
            sub += polyBlep (v.subPhase, subInc);
            sub -= polyBlep (wrap (v.subPhase + 0.5f), subInc);

            const float osc = settings.sawLevel * saw + settings.pulseLevel * pulse
                              + settings.subLevel * sub + settings.noiseLevel * noise.nextBipolar();
            out += osc * v.env;
            envSum += v.env;

            v.phase = wrap (v.phase + v.inc);
            v.phase2 = wrap (v.phase2 + v.inc2);
            v.subPhase = wrap (v.subPhase + subInc);
        }

        activitySmoother.setTarget (std::min (envSum, 1.0f));
        activitySmoother.next();
        return out * kVoiceGain;
    }

    // Smoothed 0..1 measure of "keys are sounding"; gates noise/air paths.
    float getActivity() const { return activitySmoother.current; }

    int getActiveVoiceCount() const
    {
        int n = 0;
        for (const auto& v : voices)
            n += v.active ? 1 : 0;
        return n;
    }

private:
    struct Voice
    {
        bool active = false, releasing = false, sustained = false;
        int note = -1;
        uint32_t age = 0;
        float pitch = 60.0f, targetPitch = 60.0f;
        float env = 0.0f;
        float phase = 0.0f, phase2 = 0.0f, subPhase = 0.0f;
        float inc = 0.0f, inc2 = 0.0f;
    };

    static constexpr float kVoiceGain = 0.3f;

    static inline float wrap (float p) { return p >= 1.0f ? p - 1.0f : p; }

    static inline float polyBlep (float t, float dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    void startVoice (Voice& v, int note)
    {
        const bool wasSounding = v.active;
        v.active = true;
        v.releasing = false;
        v.sustained = false;
        v.note = note;
        v.age = ++ageCounter;
        v.targetPitch = static_cast<float> (note);

        if (settings.glideSeconds > 0.0f)
            v.pitch = wasSounding ? v.pitch : lastNotePitch;
        else
            v.pitch = v.targetPitch;

        if (! wasSounding)
        {
            // Free-running analog oscillators: random start phase per note.
            v.env = 0.0f;
            v.phase = noise.nextUnipolar();
            v.phase2 = noise.nextUnipolar();
            v.subPhase = 0.5f * noise.nextUnipolar();
            v.inc = v.inc2 = 0.0f;
        }
        lastNotePitch = v.targetPitch;
    }

    Voice& findVoiceFor (int note)
    {
        for (auto& v : voices) // same key retriggers its own voice
            if (v.active && v.note == note)
                return v;
        for (auto& v : voices)
            if (! v.active)
                return v;

        Voice* oldest = nullptr; // prefer stealing the oldest released voice
        for (auto& v : voices)
            if (v.releasing && (oldest == nullptr || v.age < oldest->age))
                oldest = &v;
        if (oldest != nullptr)
            return *oldest;

        oldest = &voices[0];
        for (auto& v : voices)
            if (v.age < oldest->age)
                oldest = &v;
        return *oldest;
    }

    void noteOn (int note)
    {
        pushHeld (note);
        if (settings.mono)
            startVoice (voices[0], note);
        else
            startVoice (findVoiceFor (note), note);
    }

    void releaseVoicesPlaying (int note, size_t firstVoice)
    {
        for (size_t i = firstVoice; i < voices.size(); ++i)
        {
            auto& v = voices[i];
            if (v.active && ! v.releasing && v.note == note)
            {
                if (sustainPedal)
                    v.sustained = true;
                else
                    v.releasing = true;
            }
        }
    }

    void noteOff (int note)
    {
        removeHeld (note);

        if (settings.mono)
        {
            // Voices left over from poly mode (switched while holding a chord) still need releasing.
            releaseVoicesPlaying (note, 1);

            auto& v = voices[0];
            if (heldCount > 0)
            {
                // Last-note priority: fall back to the previous held key, legato.
                const int previous = held[static_cast<size_t> (heldCount - 1)];
                v.targetPitch = static_cast<float> (previous);
                v.note = previous;
                if (settings.glideSeconds <= 0.0f)
                    v.pitch = v.targetPitch;
            }
            else if (sustainPedal)
                v.sustained = true;
            else
                v.releasing = true;
            return;
        }

        releaseVoicesPlaying (note, 0);
    }

    void setSustain (bool down)
    {
        sustainPedal = down;
        if (down)
            return;
        for (auto& v : voices)
            if (v.active && v.sustained && ! isHeld (v.note))
            {
                v.sustained = false;
                v.releasing = true;
            }
    }

    void allNotesOff()
    {
        heldCount = 0;
        sustainPedal = false;
        for (auto& v : voices)
            if (v.active)
                v.releasing = true;
    }

    void pushHeld (int note)
    {
        removeHeld (note);
        if (heldCount < static_cast<int> (held.size()))
            held[static_cast<size_t> (heldCount)] = note;
        heldCount = std::min (heldCount + 1, static_cast<int> (held.size()));
    }

    void removeHeld (int note)
    {
        int w = 0;
        for (int r = 0; r < heldCount; ++r)
            if (held[static_cast<size_t> (r)] != note)
                held[static_cast<size_t> (w++)] = held[static_cast<size_t> (r)];
        heldCount = w;
    }

    bool isHeld (int note) const
    {
        for (int i = 0; i < heldCount; ++i)
            if (held[static_cast<size_t> (i)] == note)
                return true;
        return false;
    }

    float sampleRate = 44100.0f;
    Settings settings;
    std::array<Voice, kMaxVoices> voices {};
    std::array<int, 32> held {};
    int heldCount = 0;
    bool sustainPedal = false;
    float pitchBend = 0.0f, modWheel = 0.0f, vibratoPhase = 0.0f, lastNotePitch = 60.0f;
    float attackStep = 1.0f, releaseCoef = 0.99f, detuneMix = 1.0f;
    uint32_t ageCounter = 0;
    Random noise;
    SmoothedValue activitySmoother;
};
} // namespace vv
