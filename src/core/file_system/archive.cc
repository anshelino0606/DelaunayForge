#include "archive.h"
#include "file_system.h"
#include "core/utils.h"
#include <vector>

namespace fem {

constexpr uint64_t g_archive_version = 1;

Archive::Archive() : mode_(Mode::WRITE) {
    create_empty();
}

Archive::Archive(const std::string& path, Mode mode)
    : path_(path), mode_(mode)
{
    switch (mode_) {
    case Mode::WRITE: {
        create_empty();
        break;
    }
    case Mode::READ: {
        if (path_.empty()) {
            fail_once_("Archive::open", "empty path");
            return;
        }

        std::vector<uint8_t> file_data;

        FileSystem::read(path, file_data);
        if (file_data.empty()) {
            fail_once_("Archive::open", "failed to read file (missing/empty?)");
            return;
        }
        if (file_data.size() < sizeof(Header)) {
            fail_once_("Archive::open", "file too small to contain header");
            return;
        }

        memcpy(&header_, file_data.data(), sizeof(Header));

        // Basic validation to avoid allocating absurd buffers on corrupted files.
        constexpr uint64_t kMaxDecompressedBytes = 512ull * 1024ull * 1024ull; // 512 MB safety cap
        if (header_.decompressedSize == 0 || header_.decompressedSize > kMaxDecompressedBytes) {
            fail_once_("Archive::open", "invalid decompressed size (corrupted/unsupported archive?)");
            return;
        }

        const uint64_t payload_bytes = (uint64_t)file_data.size() - (uint64_t)sizeof(Header);
        if (header_.compressedSize == 0 || header_.compressedSize > payload_bytes) {
            fail_once_("Archive::open", "invalid compressed size (corrupted archive)");
            return;
        }

        data_.resize(header_.decompressedSize);
        uint64_t offset = sizeof(Header);
        Utils::decompress(file_data, data_, offset);

        if (data_.empty()) {
            fail_once_("Archive::open", "decompression produced empty buffer");
            return;
        }

        break;
    }
    case Mode::READ_HEADER_ONLY: {
        if (path_.empty()) {
            fail_once_("Archive::open_header", "empty path");
            return;
        }

        std::vector<uint8_t> file_data;
        FileSystem::read(path_, file_data);
        if (file_data.size() < sizeof(Header)) {
            fail_once_("Archive::open_header", "file too small to contain header");
            return;
        }
        std::memcpy(&header_, file_data.data(), sizeof(Header));

        break;
    }
    }
}

void Archive::save(const std::string& path) {
    if (!ok_) {
        fail_once_("Archive::save", "archive is in failed state");
        return;
    }
    if (data_.empty()) {
        fail_once_("Archive::save", "cannot save empty buffer");
        return;
    }
    if (path.empty()) {
        fail_once_("Archive::save", "empty output path");
        return;
    }

    std::vector<uint8_t> compressed_data;
    header_.compressedSize = Utils::compress(data_, compressed_data);
    header_.decompressedSize = data_.size();

    uint64_t offset = 0;
    std::vector<uint8_t> general_data(sizeof(Header) + compressed_data.size());
    
    memcpy(general_data.data(), &header_, sizeof(Header));
    offset += sizeof(Header);

    memcpy(general_data.data() + offset, compressed_data.data(), compressed_data.size());

    FileSystem::write(path, general_data);
}

void Archive::create_empty() {
    data_.resize(128);
    header_.version = g_archive_version;
    ok_ = true;
    error_.clear();
    error_logged_ = false;
}

}