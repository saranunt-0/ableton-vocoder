# Architecture

## Signal flow (VocoderEngine, mono core, stereo out)

```
voice ─► input gain ─► 80 Hz HP ─┬─► gate follower ─► soft-knee gate gain ───────────────┐
                                 ├─► HP 4.5k / LP 1.5k followers ─► unvoiced weight ──┐   │
                                 ├─► 2×HP 6.5k ("Air") ── × air × keys × gate ────────┼───┼──┐
                                 └─► N × [BP 4th-order ─► RMS follower] = band envs ──┼───┤  │
                                                                                      │   │  │
MIDI ─► CarrierSynth (12 voices: saw+detune, pulse, sub, noise; AR env, glide, vib) ─┤   │  │
             └─ + white noise × sibilance × unvoiced × keys ◄─────────────────────────┘   │  │
                    │                                                                     │  │
                    └─► N × [× env_b × bandGain_b ─► BP 4th-order @ f_b·formant] ─► Σ ◄───┘  │
                                                                     × makeup ─► + air ◄─────┘
                                                                        × output ─► Ensemble (3 BBD taps, L/C/R)
                                                                                  ─► + dry voice ─► soft clip ─► L/R
```

- Bands: log-spaced 180 Hz – 7.5 kHz (Sennheiser VSM-201 span), 8–24 bands.
  Section Q = Q(-3 dB at band spacing) × 0.6436 (cascade correction) × Resonance.
- The envelope multiplies the carrier *before* the carrier band filter, so envelope
  ripple sidebands are filtered out (same trick Surge XT uses).
- Band gains = sqrt(N) (band-count compensation) × Brightness tilt around 1 kHz.
- Noise ("Sibilance") and "Air" are multiplied by carrier activity so nothing
  leaks out when no key is held.
- Processing in 32-sample chunks split at MIDI event offsets: sample-accurate notes,
  per-chunk pitch/glide/vibrato and formant-smoothing updates.

## Why two plugin builds

Ableton gives an *instrument* MIDI from its own track and extra audio only through
a VST3 **aux** (sidechain) bus, while an *audio effect* gets audio from its track and
MIDI only via another track's "MIDI To". One codebase with `VV_INSTRUMENT=1/0`:

| | Vintage Vocoder | Vintage Vocoder FX |
|---|---|---|
| VST3 category | Instrument/Synth | Fx |
| Audio input bus 0 | "Voice (Sidechain)", **kAux** (via `VST3ClientExtensions::getPluginHasMainInput() = false`) | "Voice", kMain |
| AU type | aumu | aumf |
| Standalone | no (sidechain has no standalone input) | yes |

Verified with a raw VST3 probe (see plugin-wrapper timeline).

## Base-code evaluation (2026-10-10)

| Candidate | Licence | Verdict |
|---|---|---|
| Surge XT `VocoderEffect` | GPL-3 | High quality, but tied to Surge SIMD/storage infra; no MIDI carrier of its own. Borrowed: VSM-201 band span, RMS follower, envelope-before-filter. |
| mda Vocoder (Paul Kellett; LV2 port by D. Robillard) | GPL-3 (LV2 port) | Classic 8/16-band design with "hi thru" and "hi band" noise; no MIDI. Borrowed the ideas, not code. |
| dbaylies/JUCEVocoder | none stated | Small 2020 demo. |
| Steinberg mda-vst3 samples | MIT-style | Does not include the vocoder (only TalkBox/JX10). |

Decision: write a clean implementation; no third-party DSP code is copied.

## Dependencies

- JUCE 8.0.9 (AGPLv3 / commercial), fetched by CMake (or `-DVV_JUCE_DIR`).
- No other runtime dependencies. Tests use no framework.
