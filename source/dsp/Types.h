#pragma once

#include <array>
#include <cstdint>

namespace oe::dsp
{

// ---------------------------------------------------------------------------
// Selectors
// ---------------------------------------------------------------------------
enum class Style : int
{
    Single,
    Dual,
    Ratio,
    PingPong,
    Quad
};
constexpr int kNumStyles = 5;

enum class Mode : int
{
    Tape,
    TapeClean,
    ReelHiFi,
    TapeNR,
    BBD,
    BBDBright,
    AnalogPlus,
    Digital,
    Clean,
    Crystal,
    LoFi,
    GhostShift,
    Pitch,
    PitchRich,
    PitchReverse,
    PitchDuck,
    PhaserDDL
};
constexpr int kNumModes = 17;

enum class Era : int
{
    Past,
    Present,
    Future
};
constexpr int kNumEras = 3;

enum class QuadFeedback : int
{
    Repeat,
    Swell
};

inline constexpr std::array<const char*, kNumStyles> kStyleNames { "Single", "Dual", "Ratio", "PingPong", "Quad" };

inline constexpr std::array<const char*, kNumModes> kModeNames {
    "Tape", "Tape Clean", "Reel HiFi", "Tape NR", "BBD", "BBD Bright", "Analog+", "Digital", "Clean",
    "Crystal", "LoFi", "Ghost Shift", "Pitch", "Pitch Rich", "Pitch Reverse", "Pitch Duck", "Phaser DDL"
};

inline constexpr std::array<const char*, kNumEras> kEraNames { "Past", "Present", "Future" };

// ---------------------------------------------------------------------------
// Engine constants
// ---------------------------------------------------------------------------
constexpr double kMaxDelaySeconds = 20.0;
constexpr double kMaxModulationSeconds = 0.15; // modulation + grain windows margin
constexpr int kMaxTaps = 8;                    // Quad: 4 pairs
constexpr int kControlInterval = 32;           // control-rate updates (absolute sample clock)
constexpr double kModeCrossfadeSeconds = 0.030;
constexpr double kTimeCrossfadeSeconds = 0.040;

// ---------------------------------------------------------------------------
// Controls shown per Mode (the UI hides the others; values are always kept)
// ---------------------------------------------------------------------------
namespace ctl
{
    enum : unsigned
    {
        Wow = 1u << 0,
        Flutter = 1u << 1,
        ModRate = 1u << 2,
        ModDepth = 1u << 3,
        FreqShift = 1u << 4,
        FreqDetune = 1u << 5,
        PitchShift = 1u << 6,
        PitchDetune = 1u << 7,
        Age = 1u << 8,
        PitchMix = 1u << 9,
        Res = 1u << 10,
        DuckMain = 1u << 11 // Duck knob shown in the main section (otherwise in "Extra")
    };
} // namespace ctl

constexpr unsigned modeControls (Mode m) noexcept
{
    using namespace ctl;
    switch (m)
    {
        case Mode::Tape:
        case Mode::TapeClean:    return Wow | Flutter | Age;
        case Mode::TapeNR:       return Wow | Flutter | Age | DuckMain;
        case Mode::ReelHiFi:
        case Mode::BBD:
        case Mode::AnalogPlus:
        case Mode::Digital:
        case Mode::Crystal:      return ModRate | ModDepth | Age;
        case Mode::BBDBright:
        case Mode::Clean:
        case Mode::LoFi:         return ModRate | ModDepth | Age | DuckMain;
        case Mode::GhostShift:   return FreqShift | FreqDetune | Age;
        case Mode::Pitch:
        case Mode::PitchReverse: return PitchShift | PitchDetune | Age;
        case Mode::PitchDuck:    return PitchShift | PitchDetune | Age | DuckMain;
        case Mode::PitchRich:    return PitchShift | PitchDetune | PitchMix;
        case Mode::PhaserDDL:    return ModRate | ModDepth | Res;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Parameter snapshot handed to the engine once per audio block (lock-free copy)
// ---------------------------------------------------------------------------
struct EngineParams
{
    float mix = 0.30f;           // 0..1
    Style style = Style::Single;
    Mode mode = Mode::Tape;
    Era era = Era::Present;

    float delayLMs = 350.0f;     // free-running times (used when sync == 0 / "ms")
    float delayRMs = 500.0f;
    int syncL = 14;              // index into kSyncNotes, 14 = 1/4
    int syncR = 14;
    double bpm = 120.0;

    float spreadMs = 0.0f;       // Single
    float ratio = 0.618f;        // Ratio (0.01..1)
    float spacing = 0.0f;        // Quad (0..1)
    std::array<bool, 4> taps { true, true, true, true };
    QuadFeedback quadFeedback = QuadFeedback::Repeat;

    float feedback = 0.35f;      // 0..2
    float width = 1.0f;          // -1..1
    float driveDb = 0.0f;        // 0..24
    float age = 0.20f;           // 0..1
    float pitchMix = 0.5f;       // 0..1 (Pitch Rich)
    float res = 0.3f;            // 0..1 (Phaser DDL)
    float diffAmount = 0.0f;     // 0..1
    float diffSize = 0.5f;       // 0..1
    float lowCutHz = 40.0f;
    float highCutHz = 8000.0f;
    float wow = 0.25f;
    float flutter = 0.15f;
    float modRateHz = 0.5f;
    float modDepth = 0.10f;
    float freqShiftHz = 5.0f;
    float freqDetuneHz = 1.0f;
    float pitchShiftSt = 12.0f;
    float pitchDetuneCents = 10.0f;
    float duck = 0.0f;           // 0..1

    /** Internal: digital modes crossfade (instead of glide) on delay-time changes. Off by default. */
    bool digitalTimeCrossfade = false;
};

} // namespace oe::dsp

#include <cmath>

namespace oe::dsp
{

/** Equal-power dry/wet law: dry = cos θ, wet = sin θ, θ = mix · π/2. */
struct MixGains
{
    float dry, wet;
};

inline MixGains equalPowerGains (float mix01) noexcept
{
    const auto m = mix01 < 0.0f ? 0.0f : (mix01 > 1.0f ? 1.0f : mix01);
    const auto theta = 1.5707963267948966 * static_cast<double> (m);
    return { static_cast<float> (std::cos (theta)), static_cast<float> (std::sin (theta)) };
}

} // namespace oe::dsp
