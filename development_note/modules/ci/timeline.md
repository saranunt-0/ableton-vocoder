# Module: ci
<!-- .github/workflows/build.yml -->

---

## [2026-10-10] Cross-platform build workflow

**Type**: `feature`
**Status**: `resolved`

### Work Done
- `dsp-tests` (Ubuntu): JUCE-free unit tests via ctest.
- `plugins` matrix: macOS 14 (universal arm64+x86_64, VST3 + AU + Standalone, ad-hoc
  codesign), Windows 2022 (VST3 + Standalone), Ubuntu 24.04 (VST3 + Standalone).
- pluginval 1.0.4 strictness 10 on every VST3; `auval -v` on both AUs (macOS).
- Packaging with ditto/7z/zip because upload-artifact drops the executable bit inside bundles.
- `release` job publishes the zips on `v*` tags.

### Checklist
- [x] Run 38023467335 (commit c35cdf1) green on all jobs: DSP tests 8 s, Linux 4 min,
      Windows 6 min, macOS 22 min (universal + LTO build is ~19 min of that)
- [x] pluginval strictness 10 passed for both VST3s on macOS, Windows and Linux
- [x] auval: "AU VALIDATION SUCCEEDED" for aumu VVoc (instrument with sidechain input)
      and aumf VVfx, including the MIDI test
- [x] Artifacts uploaded: VintageVocoder-macOS (16.7 MB), -Windows, -Linux

### Notes
- GitHub warns that actions/checkout@v4 and upload-artifact@v4 target Node 20 (forced
  onto Node 24). Harmless now; bump them when v5 majors are the norm.
