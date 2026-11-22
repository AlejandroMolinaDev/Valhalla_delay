#include <catch2/catch_test_macros.hpp>

#include "dsp/DelayEngine.h"

TEST_CASE ("Engine prepares", "[smoke]")
{
    oe::dsp::DelayEngine engine;
    REQUIRE_FALSE (engine.isPrepared());
    engine.prepare (48000.0);
    REQUIRE (engine.isPrepared());
}
