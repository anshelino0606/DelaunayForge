#pragma once

#include <string>
#include <vector>

namespace fem {

struct BaseShaderProgramCreateInfo {
    /**
     * @brief The path to the shader file relative to the application's root `/shaders/` directory.
     * * @note 
     * - Do **NOT** include the `/shaders/` prefix in this path.
     * - Do **NOT** include the file extension (e.g., `.slang`, `.hlsl`, `.glsl`).
     * * @subsection Examples
     * - If the shader is located at `/shaders/bloom.slang`:
     * @code relative_path = "bloom"; @endcode
     * * - If the shader is nested inside a subfolder, like `/shaders/postprocess/blur.hlsl`:
     * @code relative_path = "postprocess/blur"; @endcode
     */
    std::string relative_path;

    /**
     * @brief List of preprocessor defines passed to the shader compiler.
     *
     * These defines are used to enable conditional compilation paths inside the shader,
     * similar to `#define` directives.
     *
     * @note
     * - Each string should represent a single define in the form `NAME` or `NAME=VALUE`.
     * - Order usually does not matter unless the shader compiler specifies otherwise.
     *
     * @subsection Examples
     * @code
     * defines = { "USE_SHADOWS", "MAX_LIGHTS=8" };
     * @endcode
     */
    std::vector<std::string> defines;

    bool operator==(const BaseShaderProgramCreateInfo& other) const = default;
};

struct BaseShaderProgramCreateInfoHasher {
    std::size_t operator()(const BaseShaderProgramCreateInfo& info) const noexcept;
};

}
