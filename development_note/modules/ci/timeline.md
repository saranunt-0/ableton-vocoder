# Module: ci
<!-- .github/workflows/build.yml -->

---

## [2026-10-10] Cross-platform build workflow

**Type**: `feature`
**Status**: `in-progress` (first run pending)

### Work Done
- `dsp-tests` (Ubuntu): JUCE-free unit tests via ctest.
- `plugins` matrix: macOS 14 (universal arm64+x86_64, VST3 + AU + Standalone, ad-hoc
  codesign), Windows 2022 (VST3 + Standalone), Ubuntu 24.04 (VST3 + Standalone).
- pluginval 1.0.4 strictness 10 on every VST3; `auval -v` on both AUs (macOS).
- Packaging with ditto/7z/zip because upload-artifact drops the executable bit inside bundles.
- `release` job publishes the zips on `v*` tags.

### Unverified Items
- [ ] First CI run green on all three OSes (cannot run macOS/Windows locally).
- [ ] auval accepts the aumu with an input (sidechain) bus.
