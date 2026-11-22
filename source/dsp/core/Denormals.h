#pragma once

#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1) || defined(__x86_64__)
 #include <xmmintrin.h>
 #define OE_DENORMALS_SSE 1
#elif (defined(__aarch64__) || defined(__arm64__)) && ! defined(_MSC_VER)
 #define OE_DENORMALS_ARM64 1
#endif

namespace oe::dsp
{

/** Enables flush-to-zero / denormals-are-zero for the current scope.

    The plugin itself relies on juce::ScopedNoDenormals; this helper keeps the
    JUCE-free DSP library, its unit tests and the offline tools on the same
    numeric footing.
*/
class ScopedFlushDenormals
{
public:
    ScopedFlushDenormals() noexcept
    {
#if defined(OE_DENORMALS_SSE)
        saved = _mm_getcsr();
        _mm_setcsr (saved | 0x8040u); // FTZ | DAZ
#elif defined(OE_DENORMALS_ARM64)
        std::uint64_t fpcr = 0;
        asm volatile ("mrs %0, fpcr" : "=r"(fpcr));
        saved = fpcr;
        fpcr |= (1ull << 24); // FZ
        asm volatile ("msr fpcr, %0" : : "r"(fpcr));
#endif
    }

    ~ScopedFlushDenormals() noexcept
    {
#if defined(OE_DENORMALS_SSE)
        _mm_setcsr (static_cast<unsigned int> (saved));
#elif defined(OE_DENORMALS_ARM64)
        asm volatile ("msr fpcr, %0" : : "r"(saved));
#endif
    }

    ScopedFlushDenormals (const ScopedFlushDenormals&) = delete;
    ScopedFlushDenormals& operator= (const ScopedFlushDenormals&) = delete;

private:
#if defined(OE_DENORMALS_SSE)
    unsigned int saved = 0;
#elif defined(OE_DENORMALS_ARM64)
    std::uint64_t saved = 0;
#endif
};

} // namespace oe::dsp
