#ifndef FEM_COMPILE_TIME_HASH_H
#define FEM_COMPILE_TIME_HASH_H

#include <cstdint>
#include <string_view>

namespace fem {

// A prime number used in the multiplication step of the FNV algorithm.
// The FNV-1a algorithm specifies 0x100000001b3 for 64-bit hashing.
constexpr uint64_t FNV_PRIME = 0x100000001b3ull;

// The initial hash value used to seed the FNV algorithm.
// This starting value is chosen to reduce collisions and ensure good hash distribution.
constexpr uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ull;

constexpr uint64_t fnv_iterate(uint64_t hash, uint8_t c) {
    return (hash * FNV_PRIME) ^ c;
}

template<size_t index>
constexpr uint64_t compile_time_fnv1_inner(uint64_t hash, const char* str) {
    return compile_time_fnv1_inner<index - 1>(fnv_iterate(hash, uint8_t(str[index])), str);
}

template<>
constexpr uint64_t compile_time_fnv1_inner<size_t(-1)>(uint64_t hash, const char* str) {
    return hash;
}

template<size_t len>
constexpr uint64_t compile_time_fnv1(const char (&str)[len]) {
    return compile_time_fnv1_inner<len - 2>(FNV_OFFSET_BASIS, str);
}

constexpr uint64_t runtime_fnv1(std::string_view str) {
    uint64_t hash = FNV_OFFSET_BASIS;

    for (int i = str.length() - 1; i >= 0; --i) {
        char c = str[i];
        hash *= FNV_PRIME;
        hash ^= static_cast<uint8_t>(c);
    }
    return hash;
}

}

#endif // FEM_COMPILE_TIME_HASH_H
