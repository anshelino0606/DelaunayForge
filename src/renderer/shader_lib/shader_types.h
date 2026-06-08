#pragma once

#include "core/utils.h"
#include <string>
#include <vector>
#include <optional>

namespace fem {

/**
 * @brief Parameters required to locate, compile and initialize a shader
 */
struct ShaderCompileInfo {
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

    /**
    * @brief Optional shader entry point override.
    *
    * Specifies the name of the shader entry function to use when compiling or linking
    * the shader stage.
    *
    * If not provided, the system will automatically use a default entry point name
    * depending on the shader stage:
    *
    * - Vertex shader:   `vertex_main`
    * - Fragment shader: `fragment_main`
    * - Compute shader:  `kernel_main`
    *
    * @note
    * - This field should only be set when the shader uses a non-standard entry point name.
    * - If left as `std::nullopt`, the renderer/compiler will select the appropriate default
    *   based on the pipeline stage.
    * - The value is case-sensitive and must match the function name in shader code exactly.
    *
    * @subsection Examples
    * @code
    * entry_point = "vertex_gbuffer";   // custom vertex shader entry
    * entry_point = "kernel_vbuffer";   // custom compute shader entry
    * @endcode
    *
    * @warning
    * Overriding entry points incorrectly may result in shader linkage failures or
    * runtime compilation errors.
    */
    std::optional<std::string> entry_point = std::nullopt;

    bool operator==(const ShaderCompileInfo& other) const
    {
        return relative_path == other.relative_path &&
            defines == other.defines &&
            entry_point == other.entry_point;
    }
};

struct ShaderCompileInfoHasher
{
    std::size_t operator()(const ShaderCompileInfo& info) const noexcept;
};

}
