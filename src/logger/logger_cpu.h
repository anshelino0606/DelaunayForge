#ifndef LOGGER_CPU
#define LOGGER_CPU

#include <atomic>

#if defined(_MSC_VER)
  #include <intrin.h>
  #if defined(_M_IX86) || defined(_M_X64)
    #include <immintrin.h>
  #endif
#endif

namespace logger::cpu {

inline void pause() noexcept {
#if defined(_MSC_VER)
  #if defined(_M_IX86) || defined(_M_X64)
    _mm_pause();
  #elif defined(_M_ARM) || defined(_M_ARM64)
    __yield();
  #else
    _ReadWriteBarrier();
  #endif

#elif defined(__clang__) || defined(__GNUC__)
  #if defined(__i386__) || defined(__x86_64__)
    __builtin_ia32_pause();
  #elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield");
  #else
    std::atomic_signal_fence(std::memory_order_seq_cst);
  #endif

#else
  std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

inline void relax(int spins) noexcept {
  if (spins < 32) pause();
  else std::this_thread::yield();
}

}

#endif