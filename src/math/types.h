#ifndef FEM_MATH_TYPES_H
#define FEM_MATH_TYPES_H

#include <cstddef>
#include <cstdint>
#include <limits>

namespace fem {

using Real = double;
using Index = std::uint32_t;
using Count = std::uint32_t;


inline constexpr Index invalid_index = std::numeric_limits<Index>::max();

[[nodiscard]] constexpr Index to_index_or_invalid(int value) noexcept {
    return value < 0 ? invalid_index : static_cast<Index>(value);
}

[[nodiscard]] constexpr Index to_index(std::size_t value) noexcept {
    return static_cast<Index>(value);
}

template <class T>
constexpr Count to_count(T value) noexcept {
    return static_cast<Count>(value);
}

[[nodiscard]] constexpr std::size_t to_size(Index value) noexcept {
    return static_cast<std::size_t>(value);
}

[[nodiscard]] constexpr bool is_valid(Index value) noexcept {
    return value != invalid_index;
}

[[nodiscard]] constexpr bool is_valid(Index value, std::size_t upper_bound) noexcept {
    return value != invalid_index && to_size(value) < upper_bound;
}

} // namespace fem

#endif // FEM_MATH_TYPES_H
