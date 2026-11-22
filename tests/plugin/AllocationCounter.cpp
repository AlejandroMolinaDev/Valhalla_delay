// Replaces the global allocation functions of the PluginTests executable so a
// test can assert that processBlock() never touches the heap (real-time rule).

#include "AllocationCounter.h"

#include <cstdlib>
#include <new>

namespace
{
    thread_local bool counting = false;
    thread_local std::size_t allocations = 0;

    void* allocate (std::size_t size)
    {
        if (counting)
            ++allocations;
        if (size == 0)
            size = 1;
        if (auto* p = std::malloc (size))
            return p;
        throw std::bad_alloc();
    }

    void* allocateAligned (std::size_t size, std::align_val_t al)
    {
        if (counting)
            ++allocations;
        const auto alignment = static_cast<std::size_t> (al);
        if (size == 0)
            size = alignment;
#if defined(_MSC_VER)
        if (auto* p = _aligned_malloc (size, alignment))
            return p;
#else
        size = (size + alignment - 1) / alignment * alignment;
        if (auto* p = std::aligned_alloc (alignment, size))
            return p;
#endif
        throw std::bad_alloc();
    }

    void freeAligned (void* p) noexcept
    {
#if defined(_MSC_VER)
        _aligned_free (p);
#else
        std::free (p);
#endif
    }
} // namespace

void* operator new (std::size_t size) { return allocate (size); }
void* operator new[] (std::size_t size) { return allocate (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate (size); } catch (...) { return nullptr; }
}
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate (size); } catch (...) { return nullptr; }
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { std::free (p); }

void* operator new (std::size_t size, std::align_val_t al) { return allocateAligned (size, al); }
void* operator new[] (std::size_t size, std::align_val_t al) { return allocateAligned (size, al); }
void operator delete (void* p, std::align_val_t) noexcept { freeAligned (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { freeAligned (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { freeAligned (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { freeAligned (p); }

namespace oe::test
{

ScopedAllocationCounter::ScopedAllocationCounter()
{
    allocations = 0;
    counting = true;
}

ScopedAllocationCounter::~ScopedAllocationCounter() { counting = false; }

std::size_t ScopedAllocationCounter::count() const { return allocations; }

} // namespace oe::test
