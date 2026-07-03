#ifndef LOGGER_CPU
#define LOGGER_CPU

#include "core/macro.h"

#include <thread>

namespace logger::cpu {

inline void pause() noexcept {
  FEM_CPU_RELAX();
}

inline void relax(int spins) noexcept {
  if (spins < 32) pause();
  else std::this_thread::yield();
}

}

#endif