#ifndef FEM_CORE_UTILS_H
#define FEM_CORE_UTILS_H

#include <vector>

namespace fem {

class Utils {
public:
    static uint64_t compress(
        const std::vector<uint8_t>& in_data,
        std::vector<uint8_t>& out_compressed_data
    );

    // outData array must be resized
    static void decompress(
        const std::vector<uint8_t>& in_compressed_data,
        std::vector<uint8_t>& out_data,
        uint64_t compressed_data_offset = 0
    );

    static void hash_combine(std::size_t& seed, std::size_t value);
};

}

#endif // FEM_CORE_UTILS_H