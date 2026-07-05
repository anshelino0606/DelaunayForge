#ifndef LOGGER
#define LOGGER

#include "core/threading/mpsc_queue.h"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <functional>
#include "logger_cpu.h"
#include "logger_sink_fd.h"

namespace logger {

namespace mpsc = fem::threading;

enum class Level : uint8_t { Debug, Info, Warn, Error };

struct Site {
    const char* tag;   // e.g. "ALLOC", "GPU", "IO"
    const char* file;  // __FILE__
    const char* func;  // __func__
    uint32_t    line;  // __LINE__
};

inline uint64_t now_ns() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

inline uint32_t tid_hash() {
    return (uint32_t)std::hash<std::thread::id>{}(std::this_thread::get_id());
}

inline const char* level_name(Level l) {
    switch (l) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
    }
    return "UNKWN";
}

inline const char* level_color(Level l) {
    switch (l) {
        case Level::Debug: return "\x1b[90m"; // gray
        case Level::Info:  return "\x1b[32m"; // green
        case Level::Warn:  return "\x1b[33m"; // yellow
        case Level::Error: return "\x1b[31m"; // red
    }
    return "\x1b[0m";
}

template <std::size_t TextCap>
struct alignas(16) LogMsg {
    Level     level{};
    uint64_t  ts_ns{};
    uint32_t  tid{};
    Site      site{};
    uint16_t  len{};
    char      text[TextCap]{};
};

struct StderrSink {
    static void write(const char* p, std::size_t n) {
        (void)std::fwrite(p, 1, n, stderr);
    }
    static void flush() { (void)std::fflush(stderr); }
};

template <
    std::size_t QCapPow2,
    std::size_t TextCap = 256,
    bool UseColor = true,
    class SinkT = logger::BufferedStderrSink,
    int EnqueueSpins = 256
>
class Logger {
public:
    Logger() : running_(true), th_([this]{ run_(); }) {}
    ~Logger() { stop(); }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void set_min_level(Level l) { min_level_.store(l, std::memory_order_relaxed); }

    uint64_t dropped_count() const { return dropped_.load(std::memory_order_relaxed); }

    void stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false, std::memory_order_release)) return;

        wakeup_seq_.fetch_add(1, std::memory_order_release);
        wakeup_seq_.notify_one();

        if (th_.joinable()) th_.join();
    }

    void logf(Level lvl, Site site, const char* fmt, ...) {
        if ((uint8_t)lvl < (uint8_t)min_level_.load(std::memory_order_relaxed)) return;

        LogMsg<TextCap> m;
        m.level = lvl;
        m.ts_ns = now_ns();
        m.tid   = tid_hash();
        m.site  = site;

        va_list ap;
        va_start(ap, fmt);
        const int n = std::vsnprintf(m.text, TextCap, fmt, ap);
        va_end(ap);

        if (n < 0) {
            m.text[0] = '\0';
            m.len = 0;
        } else {
            const int clamped = (n >= (int)TextCap) ? (int)TextCap - 1 : n;
            m.text[clamped] = '\0';
            m.len = (uint16_t)clamped;
        }

        enqueue_or_drop_(m);
    }

private:

    SinkT sink_{};

    void enqueue_or_drop_(const LogMsg<TextCap>& m) {
        for (int spins = 0; spins < EnqueueSpins; ++spins) {
            if (q_.try_push(m)) {
                wakeup_seq_.fetch_add(1, std::memory_order_release);
                wakeup_seq_.notify_one();
                return;
            }
            logger::cpu::relax(spins);
        }
        dropped_.fetch_add(1, std::memory_order_relaxed);
        // MAYBE? notify the logger occasionally to print drop stats
        // wakeup_seq_.notify_one();
    }

    void run_() {
        char linebuf[512];
        uint32_t seen = wakeup_seq_.load(std::memory_order_acquire);

        for (;;) {
            bool did_work = false;

            LogMsg<TextCap> m;
            while (q_.try_pop(m)) {
                did_work = true;
                emit_(m, linebuf, sizeof(linebuf));
            }

            // If we stopped and there is no more work, exit
            if (!running_.load(std::memory_order_acquire)) {
                while (q_.try_pop(m)) emit_(m, linebuf, sizeof(linebuf));
                break;
            }

            // Occasionally report drops
            if (did_work) {
                const uint64_t d = dropped_.exchange(0, std::memory_order_relaxed);
                if (d != 0) {
                    const char* c0 = UseColor ? "\x1b[35m" : "";
                    const char* c1 = UseColor ? "\x1b[0m"  : "";
                    int n = std::snprintf(linebuf, sizeof(linebuf),
                                          "%s[LOGGER][DROP] dropped=%llu%s\n",
                                          c0, (unsigned long long)d, c1);
                    if (n > 0) sink_.write(linebuf, (std::size_t)n);
                }
                continue;
            }

            // Park: only sleep if no one has signaled since we last checked.
            const uint32_t cur = wakeup_seq_.load(std::memory_order_acquire);
            if (cur == seen) {
                wakeup_seq_.wait(seen, std::memory_order_relaxed);
            }
            seen = wakeup_seq_.load(std::memory_order_acquire);
        }

        sink_.flush();
    }

    void emit_(const LogMsg<TextCap>& m, char* tmp, std::size_t tmpcap) {
        const char* c0 = UseColor ? level_color(m.level) : "";
        const char* c1 = UseColor ? "\x1b[0m" : "";

        const int n = std::snprintf(
            tmp, tmpcap,
            "%s[%s][%08x][%s] %s:%u %s - %.*s%s\n",
            c0,
            level_name(m.level),
            m.tid,
            (m.site.tag ? m.site.tag : "?"),
            (m.site.file ? m.site.file : "?"),
            (unsigned)m.site.line,
            (m.site.func ? m.site.func : "?"),
            (int)m.len, m.text,
            c1
        );

        if (n > 0) {
            const std::size_t nn =
                (std::size_t)((n < (int)tmpcap) ? n : (int)tmpcap - 1);
            sink_.write(tmp, nn);
        }
    }


    std::atomic<bool>  running_{false};
    std::atomic<Level> min_level_{Level::Debug};

    alignas(64) std::atomic<uint32_t> wakeup_seq_{0};
    alignas(64) std::atomic<uint64_t> dropped_{0};

    logger::mpsc::BoundedMPSC<LogMsg<TextCap>, QCapPow2> q_{};
    std::thread th_;
};


} // namespace logger

#endif