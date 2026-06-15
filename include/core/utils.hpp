#pragma once

#include <thread>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#include <immintrin.h>
#endif

inline void cpu_pause() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
  _mm_pause();
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_ARCH)
  asm volatile("yield" ::: "memory");
#else
  std::this_thread::yield();
#endif
}
