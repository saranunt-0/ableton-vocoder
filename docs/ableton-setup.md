# Using Vintage Vocoder in Ableton Live

A vocoder needs two signals at the same time:

| Signal | What it is | Where it comes from |
|---|---|---|
| **Modulator** | your voice (it supplies the "talking") | a mic on an audio track |
| **Carrier** | the synth you play (it supplies the pitch) | your MIDI keyboard, playing the plugin's built-in synth |

Ableton routes MIDI and sidechain audio differently for instruments and for
effects, so the plugin comes in two builds. Use whichever fits your set: they
sound identical.

| Build | Lives on | Voice arrives via | MIDI arrives via |
|---|---|---|---|
| **Vintage Vocoder** (instrument) | a MIDI track | the device's **Sidechain** section | the MIDI track itself |
| **Vintage Vocoder FX** (audio effect) | the vocal audio track | the track's own input | another MIDI track's **MIDI To** |

The status line under the band meter tells you which half is missing
("No voice signal", "Voice OK. Arm this MIDI track", and so on), and the
**Listen** switch plays the carrier synth on its own, so you can check MIDI
without singing.

---

## 0. Audio preferences (once)

1. **Preferences → Audio**
   - *Driver Type*: CoreAudio (macOS) or **ASIO** (Windows; for the Ampero use the Hotone ASIO driver).
   - *Audio Input/Output Device*: your interface (e.g. Ampero II Stage).
   - *Buffer Size*: 128 samples or lower for live singing (higher = more delay in your headphones).
2. **Input Config**: enable the input pair your mic arrives on (on the Ampero, see
   [ampero-ii-stage-usb.md](ampero-ii-stage-usb.md#mic-for-the-vocoder)).
3. **Preferences → Plug-Ins**: turn on *Use VST3 Plug-in System Folders*
   (macOS AU users: *Use Audio Units*), then **Rescan**.

## 1. Mic track (both methods)

1. Create an audio track, name it **Vocal**.
2. *Audio From*: **Ext. In**, then pick the mono input your mic is on (e.g. `1` or `8`).
3. *Monitor*: **In**. (Auto also works but only while the track is armed.)
4. Talk and check that the track meter moves.

## 2a. Instrument method: Vintage Vocoder on a MIDI track (recommended)

1. Create a MIDI track and drop **Vintage Vocoder** on it (Plug-Ins → VST3/AU → saranunt).
2. *MIDI From*: your keyboard, and **arm** the track (or set Monitor to In).
3. Open the device's **Sidechain** section. Live shows it at the left edge of
   any plugin that has a sidechain input; expand it from the device title bar
   if it is collapsed. Then set:
   - *Audio From*: **Vocal**
   - tap point: **Pre FX** (so the vocal track's fader doesn't change the vocoder).
4. Stop hearing the dry voice: on the **Vocal** track set *Audio To* to
   **Sends Only** (or pull its fader down; with Pre FX the vocoder keeps working).
5. Hold a chord and talk.

## 2b. Effect method: Vintage Vocoder FX on the vocal track

1. Drop **Vintage Vocoder FX** on the **Vocal** track.
2. Create a MIDI track. *MIDI From*: your keyboard. **Arm** it.
3. *MIDI To*: **Vocal**, then in the second chooser below it, **Vintage Vocoder FX**.
   (Live only lists tracks with a device that accepts MIDI.)
4. Hold a chord and talk.

## 3. Recording

- Record the vocoder **as audio**: create an audio track with *Audio From* set
  to the track that holds the plugin (or *Resampling*), arm it, record.
- Or record your **voice and MIDI separately** (arm both tracks) and keep
  tweaking the vocoder afterwards, because it re-renders on playback.

## Starting sound

| Goal | Settings |
|---|---|
| Classic 70s strings-vocoder (VP-330 vibe) | Bands 10, Saw 100 %, Sub 0, Ensemble 70 %, Release 400 ms |
| Clear, intelligible robot | Bands 20, Resonance 1.5, Sibilance 70 %, Air 40 %, Brightness 4 |
| Daft-Punk-ish lead | Mode Mono, Glide 60 ms, Pulse 60 %, Width 25 %, Ensemble 20 % |
| Deep choir | Octave -1, Formant -4 st, Detune 15 ct, Ensemble 80 % |

## Troubleshooting

| Symptom | Fix |
|---|---|
| Status: *No voice signal* | Vocal track Monitor = In, correct *Audio From* input, Sidechain set (instrument build). |
| Status: *Voice OK. Arm this MIDI track* | Arm the MIDI track; check *MIDI From* is your keyboard and the keyboard's Track switch is on in Preferences → Link, Tempo & MIDI. |
| **Listen** makes sound but singing doesn't | The sidechain/voice routing is the problem, not MIDI. |
| Constant drone or hiss while not singing | Raise **Gate** (try -45 dB). |
| Words hard to understand | More **Bands**, more **Sibilance** and **Air**, Brightness up. |
| Too thin | Lower **Brightness**, add **Sub**, Formant slightly negative. |
| You hear yourself twice / phasey | The dry vocal is still going to the Master: Vocal track *Audio To* = Sends Only. |
| Delay between singing and sound | Lower the buffer size; avoid latency-heavy plugins before the vocoder. |
| Plugin not listed | Rescan (Preferences → Plug-Ins). On macOS remove quarantine (see `INSTALL.txt`). |

## No-install alternative: Live's own Vocoder

Live Standard/Suite includes a **Vocoder** audio effect. Put it *after* a synth
(e.g. Analog/Drift) on a MIDI track and choose the **Vocal** track in its
external-input chooser. In that position Live uses the synth as the carrier and
the external track as the modulator. It is a good reference to compare against.
Vintage Vocoder adds the built-in vintage carrier, unvoiced-noise and "air"
paths, the formant shift and the string ensemble.
