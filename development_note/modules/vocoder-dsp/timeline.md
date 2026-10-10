# Module: vocoder-dsp
<!-- JUCE-free DSP in Source/dsp: VocoderBank, CarrierSynth, Ensemble, VocoderEngine -->

---

## [2026-10-10] Initial implementation

**Type**: `feature`
**Status**: `resolved`

### Context
Empty repository. Goal: MIDI-playable vintage analog-style vocoder for Ableton.

### Work Done
- TPT SVF filters (stable under per-chunk retuning for the formant shift).
- VocoderBank: N = 8–24 bands, 2 cascaded band-pass sections per band, RMS followers
  with per-band minimum attack/release times (1 and 2 periods of the band centre).
- CarrierSynth: 12 voices, PolyBLEP saw (+ detuned saw, crossfaded in over 0–2 cents
  to avoid static phase-cancellation at 0 detune), PolyBLEP pulse and sub square,
  noise; linear attack / exponential release; poly with voice stealing (same note →
  oldest released → oldest), mono last-note legato, glide, bend, vibrato + mod wheel,
  sustain pedal, CC120/CC123.
- Engine: gate (soft knee over 6 dB), unvoiced detector (HF/LF follower ratio,
  smoothstep 0.35–1.0), Air path, ensemble, dry voice, soft clipper (linear below 0.8).
- Level calibration: first run measured -33.7 dBFS out for -20 dBFS in → makeup raised.
  12.0 gave -20.1 dBFS for one note, but the triad demo peaked at 0.84 (in the clipper
  knee), so settled on 8.0: one note ≈ -23.6 dBFS, triad demo peak 0.56 / -18.5 dBFS RMS.
  Band-count spread 3.0 dB (8 → 24 bands).

### Checklist
- [x] 15 unit tests / 57 checks pass (`ctest --test-dir build-tests`)
- [x] -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wfloat-equal clean
- [x] Finite and bounded output at 22.05–192 kHz with extreme settings + full-scale noise
- [ ] Listening test with a real voice (synthetic vowels only so far)

### Unverified Items
- [ ] `vv_render out.wav your_voice.wav` with a real 16-bit vocal: expect clearly
      intelligible words on the Am-F-C-G progression; report if sibilants are harsh
      or words are muddy (tune Sibilance/Air defaults).

### Assumptions Made
- Typical vocal level around -20 dBFS RMS when choosing the makeup gain.
- 12 voices are enough for vocoder chords; CPU cost is dominated by the bank, not voices.
