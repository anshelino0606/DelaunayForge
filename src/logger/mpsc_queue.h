#ifndef LOGGER_MPSC_QUEUE_H
#define LOGGER_MPSC_QUEUE_H

#include "core/threading/mpsc_queue.h"

namespace logger::mpsc {

inline constexpr std::size_t kCacheLine = fem::threading::kCacheLine;

template <class T, std::size_t CapacityPow2>
using BoundedMPSC = fem::threading::BoundedMPSC<T, CapacityPow2>;

} // namespace logger::mpsc

#endif // LOGGER_MPSC_QUEUE_H