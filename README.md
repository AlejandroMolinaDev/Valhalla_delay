# Orbit Echo

Multimode stereo delay plugin (tape, BBD, digital, pitch, freq-shift, phaser) — JUCE VST3 / AU / CLAP.

> Work in progress. The full user guide, DAW installation instructions and preset list arrive with v1.0.0.

## Formats and platforms

| Format | Windows | macOS (universal arm64 + x86_64, 10.13+) | Linux |
|---|---|---|---|
| VST3 | ✅ | ✅ | ✅ |
| AU | — | ✅ | — |
| CLAP | ✅ | ✅ | ✅ |
| Standalone | ✅ | ✅ | ✅ |

VST2 is not built (the SDK is not redistributable). AAX is out of scope (see "Enabling AAX" in the final README).

## Building

```bash
git clone --recursive <this repository>
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release
```

Requirements: CMake ≥ 3.25, a C++20 compiler (MSVC 2022, Xcode 15+, GCC 11+ / Clang 14+).
On Linux install the JUCE dependencies first (`libasound2-dev libx11-dev libxrandr-dev libxinerama-dev
libxcursor-dev libxcomposite-dev libxext-dev libfreetype-dev libfontconfig1-dev libgl-dev`).

## License

Orbit Echo is released under the **GNU AGPL-3.0** (see `LICENSE`), because it is built with
[JUCE 8](https://juce.com), which is dual-licensed AGPLv3 / commercial. **Distributing Orbit Echo
commercially (closed source) requires a commercial JUCE license.** CLAP support comes from
[free-audio/clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) (MIT).
