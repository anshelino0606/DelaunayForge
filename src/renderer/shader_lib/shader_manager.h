#pragma once

#include "graphics_shader_program.h"
#include <memory>
#include <unordered_map>

namespace fem {

class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    GraphicsShaderProgram* graphics_shader_program(const GraphicsShaderProgramCreateInfo& create_info);

private:
    template<typename TKey, typename TValue, typename THasher>
    using ShaderProgramMap = std::unordered_map<TKey, TValue, THasher>;

    using GraphicsShaderProgramMap = ShaderProgramMap<
        GraphicsShaderProgramCreateInfo, 
        std::unique_ptr<GraphicsShaderProgram>,
        GraphicsShaderProgramCreateInfoHasher  
    >;

    GraphicsShaderProgramMap graphics_shader_programs_;

    template<typename TKey, typename TValue, typename THasher>
    TValue::pointer get_shader_program(ShaderProgramMap<TKey, TValue, THasher>& map, const TKey& create_info) {
        auto it = map.find(create_info);
        if (it != map.end()) {
            return it->second.get();
        }

        return map.emplace(create_info, new TValue::element_type(create_info)).first->second.get();
    }
};

}