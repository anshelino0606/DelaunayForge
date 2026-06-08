#include "utils.h"

#include <lz4/lz4.h>

namespace fem {

uint64_t Utils::compress(
    const std::vector<uint8_t>& in_data,
    std::vector<uint8_t>& out_compressed_data
)
{
    uint64_t compressStaging = LZ4_compressBound(in_data.size());
    out_compressed_data.resize(compressStaging);
    uint64_t compressedDataSize = LZ4_compress_default(
        (const char*)in_data.data(), 
        (char*)out_compressed_data.data(), 
        in_data.size(), 
        compressStaging
    );
    out_compressed_data.resize(compressedDataSize);
    return compressedDataSize;
}

void Utils::decompress(
    const std::vector<uint8_t>& in_compressed_data,
    std::vector<uint8_t>& out_data,
    uint64_t compressed_data_offset
) {
    const uint8_t* compressedDataPtr = in_compressed_data.data() + compressed_data_offset;
    uint64_t compressedSize = in_compressed_data.size() - compressed_data_offset;
    LZ4_decompress_safe(
        (const char*)compressedDataPtr, 
        (char*)out_data.data(), 
        compressedSize, 
        out_data.size()
    );
}

void Utils::hash_combine(std::size_t& seed, std::size_t value) {
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

}