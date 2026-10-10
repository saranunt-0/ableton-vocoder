# Module: plugin-wrapper
<!-- Source/plugin: JUCE AudioProcessor, parameters, editor; instrument + FX builds -->

---

## [2026-10-10] JUCE wrapper, two builds, validation

**Type**: `feature`
**Status**: `resolved` (host testing in Ableton still pending)

### Context
Ableton routes MIDI + extra audio differently to instruments and effects, so the
same engine ships as `Vintage Vocoder` (instrument, voice via sidechain) and
`Vintage Vocoder FX` (effect, MIDI via "MIDI To").

### Work Done
- 28 parameters (IDs frozen in `Parameters.h`); defaults come from `vv::VocoderParams`.
- Processor: voice = mono sum of input bus 0; host blocks larger than prepareToPlay's
  size are sliced (no allocation on the audio thread; MIDI capped at 4096 events/block).
- Instrument input bus reported as VST3 kAux via `getPluginHasMainInput() = false`.
- Editor: vintage look-and-feel, band LED meter, VOICE/MIDI/KEYS LEDs and a routing
  hint line. Bug found in screenshot: value boxes had white outlines because the
  sliders were built before `setLookAndFeel`. Fixed by setting the LnF after creating
  the controls.
- Build bug found: `BusesProperties` is protected; a free helper did not compile.
  The first "successful" build only showed bundle skeletons. Moved into the class.

### Checklist
- [x] Linux build of both VST3s + FX Standalone (JUCE 8.0.9, GCC 13)
- [x] pluginval 1.0.4, strictness 10, in-process: SUCCESS for both (editor, processing,
      state restore, automation, fuzzing, thread safety, bus layouts)
- [x] Raw VST3 probe: instrument `audio in bus 0 'Voice (Sidechain)' type=AUX`, FX `type=MAIN`,
      both expose 1 event input bus
- [x] Standalone UI screenshot under Xvfb (`docs/images/vintage-vocoder-ui.png`)
- [ ] Steinberg `validator` (not bundled with pluginval)
- [ ] macOS AU (auval) and Windows build: CI only

### Unverified Items (need Ableton on the user's machine)
- [ ] Instrument: MIDI track + Sidechain = Vocal track → vocoder speaks. Expected: the
      Sidechain section is visible on the device.
- [ ] FX: Vocal track + MIDI track "MIDI To" → Vocal / Vintage Vocoder FX → vocoder speaks.
- [ ] AU instrument sidechain in Live on macOS (AU sidechain support for instruments is host-specific).
