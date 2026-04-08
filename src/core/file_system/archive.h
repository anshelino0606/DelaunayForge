#ifndef FEM_ARCHIVE_H
#define FEM_ARCHIVE_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <glm/glm.hpp>

#include "logger/logger_macros.h"

namespace fem {

inline constexpr ::logger::Category LogArchive{"Archive"};

class Archive {
public:
    enum class Mode
    {
        READ,
        WRITE,
        READ_HEADER_ONLY
    };

    struct Header {
        uint64_t version = 0;
        uint64_t compressedSize = 0;
        uint64_t decompressedSize = 0;
    };

    // Creates empty binary archive for writing
    Archive();
    Archive(const std::string& path, Mode mode = Mode::READ);

    const std::string path() const { return path_; }

    bool ok() const { return ok_; }
    const std::string& error() const { return error_; }

    void save(const std::string& path);

    const Header& get_header() const { return header_; }
    uint64_t get_version() const { return header_.version; }

    uint64_t position() const { return position_; }
    uint64_t size_bytes() const { return data_.size(); }
    uint64_t remaining_bytes() const { return position_ <= data_.size() ? (uint64_t)data_.size() - position_ : 0ull; }

    bool is_write_mode() const { return mode_ == Mode::WRITE; }
    bool is_read_mode() const { return mode_ == Mode::READ; }
    bool is_read_header_only_mode() const { return mode_ == Mode::READ_HEADER_ONLY; }

    Archive& operator<<(bool data) {
        write(static_cast<uint32_t>(data ? 1 : 0));
        return *this;
    }

    Archive& operator<<(char data) {
        write(static_cast<int8_t>(data));
        return *this;
    }

    Archive& operator<<(unsigned char data) {
        write(static_cast<uint8_t>(data));
        return *this;
    }

    Archive& operator<<(short data) {
        write(static_cast<int16_t>(data));
        return *this;
    }

    Archive& operator<<(unsigned short data) {
        write(static_cast<uint16_t>(data));
        return *this;
    }

    Archive& operator<<(int data) {
        write(static_cast<int64_t>(data));
        return *this;
    }

    Archive& operator<<(unsigned int data) {
        write(static_cast<uint64_t>(data));
        return *this;
    }

    Archive& operator<<(long data) {
        write(static_cast<int64_t>(data));
        return *this;
    }

    Archive& operator<<(unsigned long data) {
        write(static_cast<uint64_t>(data));
        return *this;
    }

    Archive& operator<<(long long data) {
        write(static_cast<int64_t>(data));
        return *this;
    }

    Archive& operator<<(unsigned long long data) {
        write(static_cast<uint64_t>(data));
        return *this;
    }

    Archive& operator<<(float data) {
        write(data);
        return *this;
    }

    Archive& operator<<(double data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::vec2& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::vec3& data) {
        write(data);
        return *this;
    }
    
    Archive& operator<<(const glm::vec4& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::dvec2& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::dvec3& data) {
        write(data);
        return *this;
    }
    
    Archive& operator<<(const glm::dvec4& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::ivec2& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::ivec3& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::ivec4& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::uvec2& data) {
        write(data);
        return *this;
    }
    
    Archive& operator<<(const glm::uvec3& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::uvec4& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::mat3x3& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::mat3x4& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::mat4x3& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const glm::mat4x4& data) {
        write(data);
        return *this;
    }

    Archive& operator<<(const std::string& data)
    {
        (*this) << data.length();
        for (const char& x : data)
            (*this) << x;
        return *this;
    }

    template<typename T>
    Archive& operator<<(const std::vector<T>& data) {
        (*this) << data.size();
        for (const T& x : data)
            (*this) << x;
        return *this;
    }

    Archive& operator>>(bool& out_data) {
        uint32_t temp;
        read(temp);
        out_data = temp == 1;
        return *this;
    }

    Archive& operator>>(char& out_data) {
        int8_t temp;
        read(temp);
        out_data = static_cast<char>(temp);
        return *this;
    }

    Archive& operator>>(unsigned char& out_data) {
        uint8_t temp;
        read(temp);
        out_data = static_cast<unsigned char>(temp);
        return *this;
    }

    Archive& operator>>(short& out_data) {
        int16_t temp;
        read(temp);
        out_data = static_cast<short>(temp);
        return *this;
    }

    Archive& operator>>(unsigned short& out_data) {
        uint16_t temp;
        read(temp);
        out_data = static_cast<unsigned short>(temp);
        return *this;
    }

    Archive& operator>>(int& out_data) {
        int64_t temp;
        read(temp);
        out_data = static_cast<int>(temp);
        return *this;
    }

    Archive& operator>>(unsigned int& out_data) {
        uint64_t temp;
        read(temp);
        out_data = static_cast<unsigned int>(temp);
        return *this;
    }

    Archive& operator>>(long& out_data) {
        int64_t temp;
        read(temp);
        out_data = static_cast<long>(temp);
        return *this;
    }

    Archive& operator>>(unsigned long& out_data) {
        uint64_t temp;
        read(temp);
        out_data = static_cast<unsigned long>(temp);
        return *this;
    }

    Archive& operator>>(long long& out_data) {
        int64_t temp;
        read(temp);
        out_data = static_cast<long long>(temp);
        return *this;
    }

    Archive& operator>>(unsigned long long& out_data) {
        uint64_t temp;
        read(temp);
        out_data = static_cast<unsigned long long>(temp);
        return *this;
    }

    Archive& operator>>(float& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(double& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::vec2& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::vec3& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::vec4& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::dvec2& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::dvec3& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::dvec4& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::ivec2& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::ivec3& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::ivec4& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::uvec2& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::uvec3& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::uvec4& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::mat3x3& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::mat3x4& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::mat4x3& out_data) {
        read(out_data);
        return *this;
    }

    Archive& operator>>(glm::mat4x4& out_data) {
        read(out_data);
        return *this;
    }

    template<typename T>
    Archive& operator>>(std::vector<T>& out_data) {
        uint64_t size;
        (*this) >> size;
        out_data.resize(size);
        
        for (uint32_t i = 0; i != size; ++i)
            (*this) >> out_data[i];

        return *this;
    }

    Archive& operator>>(std::string& out_data) {
        uint64_t len;
        (*this) >> len;
        out_data.resize(len);

        for (uint32_t i = 0; i != len; ++i)
            (*this) >> out_data[i];

        return *this;
    }

protected:
    std::string path_;
    Mode mode_ = Mode::READ;

    Header header_;
    std::vector<uint8_t> data_;
    uint64_t position_ = 0;

    bool ok_ = true;
    bool error_logged_ = false;
    std::string error_;

    void fail_once_(const char* where, const char* msg)
    {
        if (!ok_) {
            return;
        }
        ok_ = false;
        error_ = std::string(where) + ": " + msg;

        if (!error_logged_) {
            error_logged_ = true;
            LOGT_ERROR(LogArchive, "%s (path='%s' mode=%d pos=%llu size=%llu)",
                       error_.c_str(),
                       path_.c_str(),
                       (int)mode_,
                       (unsigned long long)position_,
                       (unsigned long long)data_.size());
        }
    }

    template<typename T>
    void write(const T& data)
    {
        if (!ok_) return;
        if (mode_ != Mode::WRITE) {
            fail_once_("Archive::write", "archive is not in WRITE mode");
            return;
        }
        if (data_.empty()) {
            fail_once_("Archive::write", "internal buffer is empty");
            return;
        }
        
        const uint64_t newPos = position_ + sizeof(T);
        if (newPos >= data_.size())
            data_.resize(data_.size() * 2);

        memcpy(data_.data() + position_, &data, sizeof(T));
        position_ = newPos;
    }

    template<typename T>
    void read(T& out_data)
    {
        if (!ok_) {
            out_data = T{};
            return;
        }
        if (mode_ != Mode::READ) {
            out_data = T{};
            fail_once_("Archive::read", "archive is not in READ mode");
            return;
        }
        if (data_.empty()) {
            out_data = T{};
            fail_once_("Archive::read", "internal buffer is empty");
            return;
        }
        if (position_ > data_.size() || (position_ + sizeof(T) > data_.size())) {
            out_data = T{};
            fail_once_("Archive::read", "out-of-bounds read (corrupted/invalid archive?)");
            return;
        }

        std::memcpy(&out_data, data_.data() + position_, sizeof(T));
        position_ += sizeof(T);
    }

    void create_empty();
};

}

#endif // FEM_ARCHIVE_H