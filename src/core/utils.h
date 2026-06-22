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

    template <typename T>
    requires std::is_enum_v<T>
    [[nodiscard]] static constexpr std::underlying_type_t<T> to_index(T e) noexcept {
        return static_cast<std::underlying_type_t<T>>(e);
    }
};

}

#endif // FEM_CORE_UTILS_H