// Offline renderer: vocodes a WAV file (or a synthetic vowel phrase) with a
// fixed chord progression and writes a 16-bit stereo WAV. Handy for checking
// the sound without a DAW.
//
//   vv_render out.wav                 synthetic "a-e-i-o-u" + "s" phrase
//   vv_render out.wav voice.wav       your own mono/stereo 16-bit PCM WAV

#include "../Source/dsp/VocoderEngine.h"
#include "../tests/TestSignals.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace
{
constexpr float kSr = 48000.0f;

bool readWav16 (const char* path, std::vector<float>& out, float& sampleRate)
{
    std::ifstream f (path, std::ios::binary);
    if (! f)
        return false;
    char riff[12];
    f.read (riff, 12);
    if (std::strncmp (riff, "RIFF", 4) != 0 || std::strncmp (riff + 8, "WAVE", 4) != 0)
        return false;

    uint16_t channels = 0, bits = 0;
    uint32_t rate = 0;
    while (f)
    {
        char id[4];
        uint32_t size = 0;
        f.read (id, 4);
        f.read (reinterpret_cast<char*> (&size), 4);
        if (! f)
            break;
        if (std::strncmp (id, "fmt ", 4) == 0)
        {
            std::vector<char> fmt (size);
            f.read (fmt.data(), size);
            std::memcpy (&channels, fmt.data() + 2, 2);
            std::memcpy (&rate, fmt.data() + 4, 4);
            std::memcpy (&bits, fmt.data() + 14, 2);
        }
        else if (std::strncmp (id, "data", 4) == 0)
        {
            if (bits != 16 || channels == 0)
                return false;
            std::vector<int16_t> pcm (size / 2);
            f.read (reinterpret_cast<char*> (pcm.data()), static_cast<std::streamsize> (size));
            out.resize (pcm.size() / channels);
            for (size_t i = 0; i < out.size(); ++i)
            {
                float acc = 0.0f;
                for (uint16_t c = 0; c < channels; ++c)
                    acc += static_cast<float> (pcm[i * channels + c]) / 32768.0f;
                out[i] = acc / static_cast<float> (channels);
            }
            sampleRate = static_cast<float> (rate);
            return true;
        }
        else
            f.seekg (size + (size & 1), std::ios::cur);
    }
    return false;
}

void writeWav16 (const char* path, const std::vector<float>& l, const std::vector<float>& r, float sampleRate)
{
    std::ofstream f (path, std::ios::binary);
    const uint32_t frames = static_cast<uint32_t> (l.size());
    const uint32_t dataSize = frames * 4;
    const uint32_t rate = static_cast<uint32_t> (sampleRate);
    const uint32_t byteRate = rate * 4;
    const uint32_t riffSize = 36 + dataSize;
    const uint32_t fmtSize = 16;
    const uint16_t pcm = 1, channels = 2, align = 4, bits = 16;

    f.write ("RIFF", 4);
    f.write (reinterpret_cast<const char*> (&riffSize), 4);
    f.write ("WAVEfmt ", 8);
    f.write (reinterpret_cast<const char*> (&fmtSize), 4);
    f.write (reinterpret_cast<const char*> (&pcm), 2);
    f.write (reinterpret_cast<const char*> (&channels), 2);
    f.write (reinterpret_cast<const char*> (&rate), 4);
    f.write (reinterpret_cast<const char*> (&byteRate), 4);
    f.write (reinterpret_cast<const char*> (&align), 2);
    f.write (reinterpret_cast<const char*> (&bits), 2);
    f.write ("data", 4);
    f.write (reinterpret_cast<const char*> (&dataSize), 4);
    for (uint32_t i = 0; i < frames; ++i)
        for (float v : { l[i], r[i] })
        {
            const auto s = static_cast<int16_t> (std::clamp (v, -1.0f, 1.0f) * 32767.0f);
            f.write (reinterpret_cast<const char*> (&s), 2);
        }
}

// Five vowels with a short "s" between each, half a second apart.
std::vector<float> syntheticPhrase()
{
    const vvtest::Vowel vowels[] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
                                     { 570, 840, 2410 }, { 300, 870, 2240 } };
    std::vector<float> phrase;
    for (const auto& v : vowels)
    {
        auto vowel = vvtest::vowel (v, 130.0f, kSr, static_cast<int> (kSr * 0.6f), -20.0f);
        auto sss = vvtest::fricative (kSr, static_cast<int> (kSr * 0.15f), -28.0f);
        for (size_t i = 0; i < 480; ++i) // short fades to avoid clicks
        {
            const float g = static_cast<float> (i) / 480.0f;
            vowel[i] *= g;
            vowel[vowel.size() - 1 - i] *= g;
        }
        phrase.insert (phrase.end(), vowel.begin(), vowel.end());
        phrase.insert (phrase.end(), sss.begin(), sss.end());
    }
    phrase.resize (phrase.size() + static_cast<size_t> (kSr * 0.5f), 0.0f);
    return phrase;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf ("usage: %s out.wav [voice.wav]\n", argv[0]);
        return 1;
    }

    float sampleRate = kSr;
    std::vector<float> voice;
    if (argc >= 3)
    {
        if (! readWav16 (argv[2], voice, sampleRate))
        {
            std::printf ("could not read %s (16-bit PCM WAV only)\n", argv[2]);
            return 1;
        }
    }
    else
        voice = syntheticPhrase();

    vv::VocoderEngine engine;
    engine.setParams (vv::VocoderParams {});
    engine.prepare (sampleRate, 512);

    // Am - F - C - G, one chord per second, held across the whole input.
    const int chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 48, 52, 55 }, { 55, 59, 62 } };
    std::vector<vv::MidiEvent> events;
    const int chordLength = static_cast<int> (sampleRate);
    const int total = static_cast<int> (voice.size());
    for (int start = 0, c = 0; start < total; start += chordLength, c = (c + 1) % 4)
    {
        if (start > 0)
            for (int n : chords[(c + 3) % 4])
                events.push_back ({ start, 0x80, static_cast<uint8_t> (n), 0 });
        for (int n : chords[c])
            events.push_back ({ start, 0x90, static_cast<uint8_t> (n), 100 });
    }

    std::vector<float> left (voice.size()), right (voice.size());
    constexpr int block = 512;
    size_t ev = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        std::vector<vv::MidiEvent> blockEvents;
        while (ev < events.size() && events[ev].sampleOffset < pos + n)
        {
            auto e = events[ev++];
            e.sampleOffset -= pos;
            blockEvents.push_back (e);
        }
        engine.process (voice.data() + pos, left.data() + pos, right.data() + pos, n, blockEvents.data(),
                        static_cast<int> (blockEvents.size()));
    }

    writeWav16 (argv[1], left, right, sampleRate);
    std::printf ("wrote %s (%.1f s, peak %.2f, rms %.1f dBFS)\n", argv[1], static_cast<double> (total) / sampleRate,
                 static_cast<double> (std::max (vvtest::peak (left), vvtest::peak (right))),
                 static_cast<double> (vv::gainToDb (vvtest::rms (left))));
    return 0;
}
