#ifndef LOGGER_SINK_FD
#define LOGGER_SINK_FD

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)
  #include <io.h>
  using ssize_t = long long;
#else
  #include <unistd.h>
#endif

namespace logger {

struct FdWriter {
    int fd{2};

    inline bool write_all(const char* p, std::size_t n) noexcept {
        while (n) {
#if defined(_WIN32)
            const int w = ::_write(fd, p, (unsigned)n);
            if (w <= 0) return false;
#else
            const ssize_t w = ::write(fd, p, n);
            if (w <= 0) return false;
#endif
            p += (std::size_t)w;
            n -= (std::size_t)w;
        }
        return true;
    }
};

template <class Writer, std::size_t BufSize>
class BufWriter {
public:
    explicit BufWriter(Writer w = Writer{}) noexcept : w_(w) {}

    inline void write(const char* p, std::size_t n) noexcept {
        if (!p || n == 0) return;

        if (n >= BufSize) {
            flush();
            (void)w_.write_all(p, n);
            return;
        }

        if (used_ + n > BufSize) flush();

        std::memcpy(buf_ + used_, p, n);
        used_ += n;

        if (buf_[used_ - 1] == '\n') flush();
    }

    inline void flush() noexcept {
        if (used_ == 0) return;
        (void)w_.write_all(buf_, used_);
        used_ = 0;
    }

    ~BufWriter() { flush(); }

private:
    alignas(64) char buf_[BufSize];
    std::size_t used_{0};
    Writer w_;
};

using BufferedStderrSink = BufWriter<FdWriter, 64 * 1024>;

} // namespace logger


#endif