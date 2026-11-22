#pragma once

#include <array>
#include <string_view>

namespace oe::dsp
{

/** Delay Sync choices. Index 0 is "ms" (free-running); the others are note
    values relative to a quarter-note beat (4/4 bars), each with its triplet (T,
    x 2/3) and dotted (D, x 1.5) variant. */
struct SyncNote
{
    const char* name;
    double beats; // quarter notes
};

inline constexpr std::array<SyncNote, 28> kSyncNotes { {
    { "ms", 0.0 },
    { "1/64T", 0.0625 * 2.0 / 3.0 }, { "1/64", 0.0625 }, { "1/64D", 0.0625 * 1.5 },
    { "1/32T", 0.125 * 2.0 / 3.0 },  { "1/32", 0.125 },  { "1/32D", 0.125 * 1.5 },
    { "1/16T", 0.25 * 2.0 / 3.0 },   { "1/16", 0.25 },   { "1/16D", 0.25 * 1.5 },
    { "1/8T", 0.5 * 2.0 / 3.0 },     { "1/8", 0.5 },     { "1/8D", 0.5 * 1.5 },
    { "1/4T", 1.0 * 2.0 / 3.0 },     { "1/4", 1.0 },     { "1/4D", 1.0 * 1.5 },
    { "1/2T", 2.0 * 2.0 / 3.0 },     { "1/2", 2.0 },     { "1/2D", 2.0 * 1.5 },
    { "1 bar T", 4.0 * 2.0 / 3.0 },  { "1 bar", 4.0 },   { "1 bar D", 4.0 * 1.5 },
    { "2 bars T", 8.0 * 2.0 / 3.0 }, { "2 bars", 8.0 },  { "2 bars D", 8.0 * 1.5 },
    { "4 bars T", 16.0 * 2.0 / 3.0 },{ "4 bars", 16.0 }, { "4 bars D", 16.0 * 1.5 },
} };

constexpr int kSyncFree = 0;
constexpr int kSyncQuarter = 14;

/** Fallback when the host does not report a tempo. */
constexpr double kDefaultBpm = 120.0;

inline double sanitiseBpm (double bpm) noexcept
{
    return (bpm >= 20.0 && bpm <= 999.0) ? bpm : kDefaultBpm;
}

/** Note value -> milliseconds. Returns a negative value for "ms" (free mode). */
inline double syncToMs (int index, double bpm) noexcept
{
    if (index <= kSyncFree || index >= static_cast<int> (kSyncNotes.size()))
        return -1.0;
    return kSyncNotes[static_cast<std::size_t> (index)].beats * 60000.0 / sanitiseBpm (bpm);
}

inline int syncIndexOf (std::string_view name) noexcept
{
    for (std::size_t i = 0; i < kSyncNotes.size(); ++i)
        if (name == kSyncNotes[i].name)
            return static_cast<int> (i);
    return -1;
}

/** Effective delay (ms) for one channel: the synced note value or the free time. */
inline double effectiveDelayMs (float freeMs, int syncIndex, double bpm) noexcept
{
    const auto synced = syncToMs (syncIndex, bpm);
    return synced >= 0.0 ? synced : static_cast<double> (freeMs);
}

} // namespace oe::dsp
