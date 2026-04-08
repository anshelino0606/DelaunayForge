#ifndef LOGGER_GLOBAL
#define LOGGER_GLOBAL
#include "logger.h"

namespace logger {
using GlobalLogger = logger::Logger<4096, 256, true, logger::BufferedStderrSink>;
inline std::atomic<bool>& alive_flag() {
    static std::atomic<bool> alive{true};
    return alive;
}
inline GlobalLogger& global() {
    static GlobalLogger inst;
    return inst;
}
inline void init(Level min = Level::Debug) { global().set_min_level(min); }
inline void shutdown() { global().stop(); }
} // namespace logger

#endif
