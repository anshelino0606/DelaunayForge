#ifndef FEM_CORE_THREADING_THREADING_CONSTANTS_H
#define FEM_CORE_THREADING_THREADING_CONSTANTS_H

#include <cstddef>

namespace fem::threading::constants {

inline constexpr std::size_t kCacheLineSize = 64;
inline constexpr std::size_t kSpinPauseThreshold = 32;
inline constexpr std::size_t kIdleBackoffSpins = 64;
inline constexpr std::size_t kDefaultLocalQueueCapacity = 1024;
inline constexpr std::size_t kDefaultRemoteQueueCapacity = 1024;
inline constexpr std::size_t kTaskPoolBlockSize = 256;
inline constexpr std::size_t kTaskPoolBlocksPerSlab = 256;
inline constexpr std::size_t kTaskPoolLocalCacheRefillCount = 32;
inline constexpr std::size_t kTaskPoolLocalCacheHighWatermark = 128;

} // namespace fem::threading::constants

#endif // FEM_CORE_THREADING_THREADING_CONSTANTS_H
