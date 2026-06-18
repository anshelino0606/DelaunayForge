#include "shader_types.h"
#include "core/utils.h"

namespace fem {

std::size_t BaseShaderProgramCreateInfoHasher::operator()(const BaseShaderProgramCreateInfo& info) const noexcept {
    std::size_t seed = 0;

    Utils::hash_combine(seed, std::hash<std::string>{}(info.relative_path));

    for (const auto& define : info.defines)
        Utils::hash_combine(seed, std::hash<std::string>{}(define));

    return seed;
}

}