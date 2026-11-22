#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace oe;

TEST_CASE ("Engine prepares, allocates 20 s and processes any block size", "[smoke]")
{
    dsp::DelayEngine engine;
    REQUIRE_FALSE (engine.isPrepared());
    engine.prepare (48000.0);
    REQUIRE (engine.isPrepared());

    // Zero-length and null calls are harmless.
    engine.process (nullptr, nullptr, 0);
    float l = 0.0f, r = 0.0f;
    engine.process (&l, &r, 0);
    engine.process (&l, &r, 1);
    REQUIRE (std::isfinite (l));

    // A 20 s delay can be requested at 192 kHz.
    dsp::DelayEngine big;
    big.prepare (192000.0);
    auto p = test::cleanDigital (20000.0f);
    big.setParameters (p);
    test::Stereo s (64);
    test::render (big, s);
    REQUIRE (big.getTapDelaySamples (0) == 20.0 * 192000.0);
}
