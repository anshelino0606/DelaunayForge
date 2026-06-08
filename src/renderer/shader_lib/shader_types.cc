#include "shader_types.h"
#include "core/utils.h"

namespace fem {

std::size_t ShaderCompileInfoHasher::operator()(const ShaderCompileInfo& info) const noexcept
{
    std::size_t seed = 0;

    Utils::hash_combine(seed, std::hash<std::string>{}(info.relative_path));

    for (const auto& define : info.defines)
        Utils::hash_combine(seed, std::hash<std::string>{}(define));

    Utils::hash_combine(
        seed,
        info.entry_point != std::nullopt
            ? std::hash<std::string>{}(*info.entry_point)
            : 0
    );

    return seed;
}

}