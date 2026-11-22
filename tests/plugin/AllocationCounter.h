#pragma once

#include <cstddef>

namespace oe::test
{

/** Counts heap allocations made on the current thread while active.
    Implemented by replacing the global operator new in AllocationCounter.cpp. */
class ScopedAllocationCounter
{
public:
    ScopedAllocationCounter();
    ~ScopedAllocationCounter();

    std::size_t count() const;
};

} // namespace oe::test
