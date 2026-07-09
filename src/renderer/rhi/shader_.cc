#include "shader_.h"
#include "log_categories.h"
#include "renderer/device.h"

#include <slang.h>
#include <slang-com-ptr.h>

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace fem {

#if defined(__APPLE__)

namespace {

std::string_view metal_stage_attribute(LLGL::ShaderType type) {
    switch (type) {
        case LLGL::ShaderType::Vertex: return "[[vertex]]";
        case LLGL::ShaderType::Fragment: return "[[fragment]]";
        case LLGL::ShaderType::Compute: return "[[kernel]]";
        default: return {};
    }
}

bool is_metal_identifier_character(char character) {
    const unsigned char value = static_cast<unsigned char>(character);
    LOGT_DEBUG(LogRenderer, "is_metal_identifier_character: char=%c value=%u", character, value);
    return std::isalnum(value) != 0 || character == '_';
}

std::string resolve_metal_entry_point(
    const std::string& source,
    LLGL::ShaderType type,
    const std::string& fallback
) {
    const std::string_view stage_attribute = metal_stage_attribute(type);
    if (stage_attribute.empty()) {
        return fallback;
    }

    const std::size_t attribute_position = source.find(stage_attribute);
    if (attribute_position == std::string::npos) {
        return fallback;
    }

    const std::size_t parameter_list_position = source.find('(', attribute_position + stage_attribute.size());
    if (parameter_list_position == std::string::npos) {
        return fallback;
    }

    std::size_t identifier_end = parameter_list_position;
    LOG_DEBUG(LogRenderer, "resolve_metal_entry_point: found stage attribute [%s] at position %zu, source: %s", stage_attribute.data(), attribute_position, source.substr(attribute_position, identifier_end - attribute_position).c_str());
    while (identifier_end > attribute_position &&
           std::isspace(static_cast<unsigned char>(source[identifier_end - 1])) != 0) {
        --identifier_end;
    }

    std::size_t identifier_begin = identifier_end;
    while (identifier_begin > attribute_position &&
           is_metal_identifier_character(source[identifier_begin - 1])) {
        --identifier_begin;
    }

    if (identifier_begin == identifier_end) {
        return fallback;
    }

    return source.substr(identifier_begin, identifier_end - identifier_begin);
}

void shift_metal_graphics_buffer_indices(std::string& source) {
    std::size_t position = 0;
    while ((position = source.find("[[buffer(", position)) != std::string::npos) {
        constexpr std::string_view prefix = "[[buffer(";
        const std::size_t index_begin = position + prefix.size();
        std::size_t index_end = index_begin;
        LOG_DEBUG(LogRenderer, "shift_metal_graphics_buffer_indices: found buffer attribute at position %zu, static_cast<unsigned char>(source[position])=%u", position, static_cast<unsigned char>(source[position]));
        while (index_end < source.size() &&
               std::isdigit(static_cast<unsigned char>(source[index_end])) != 0) {
            ++index_end;
        }

        if (index_begin == index_end) {
            position = index_end;
            continue;
        }

        const uint32_t index = static_cast<uint32_t>(
            std::stoul(source.substr(index_begin, index_end - index_begin))
        );
        LOG_DEBUG(LogRenderer, "shift_metal_graphics_buffer_indices: found buffer index %u", index);
        const std::string shifted_index = std::to_string(index + 1);
        source.replace(index_begin, index_end - index_begin, shifted_index);
        position = index_begin + shifted_index.size();
    }
}

} // namespace

#endif

Shader::Shader(const InitInfo& info) {
    create(info);
}

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept {
    handle_ = other.handle_;
    other.handle_ = nullptr;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (handle_) g_device->Release(*handle_);
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

void Shader::destroy() {
    if (handle_) {
        g_device->Release(*handle_);
        handle_ = nullptr;
    }
}

void Shader::create(const InitInfo& info) {
    destroy();

    const std::string debug_name(info.debug_name);
    const std::string requested_entry_point(info.entry_point);
    std::string resolved_entry_point = requested_entry_point;

    if (!g_device) {
        LOGT_ERROR(LogRenderer, "Cannot create shader [%s:%s]: render device is null",
                   debug_name.c_str(), requested_entry_point.c_str());
        return;
    }

    if (!info.data || info.data_size == 0) {
        LOGT_ERROR(LogRenderer, "Cannot create shader [%s:%s]: compiled shader data is empty",
                   debug_name.c_str(), requested_entry_point.c_str());
        return;
    }

    LLGL::ShaderDescriptor shader_desc;
    shader_desc.type = info.type;
    shader_desc.debugName = debug_name.c_str();

#if defined(_WIN32)
    shader_desc.entryPoint = resolved_entry_point.c_str();
    shader_desc.source = static_cast<const char*>(info.data);
    shader_desc.sourceSize = info.data_size;
    shader_desc.sourceType = LLGL::ShaderSourceType::BinaryBuffer;
#elif defined(__APPLE__)
    std::string metal_source(static_cast<const char*>(info.data), info.data_size);
    LOG_DEBUG(LogRenderer, "Shader::create: metal_source size=%zu, string: %s, info.data: %p, info.data_size: %zu", metal_source.size(), metal_source.c_str(), info.data, info.data_size);
    if (!metal_source.empty() && metal_source.back() == '\0') {
        metal_source.pop_back();
    }

    resolved_entry_point = resolve_metal_entry_point(metal_source, info.type, requested_entry_point);
    if (info.type == LLGL::ShaderType::Vertex || info.type == LLGL::ShaderType::Fragment) {
        /*
         * LLGL binds vertex input buffers starting at Metal buffer(0). Keep
         * explicit graphics shader buffers one slot higher to match the
         * renderer's Metal pipeline layouts.
         */
        shift_metal_graphics_buffer_indices(metal_source);
    }
    // log metal source
    LOG_DEBUG(LogRenderer, "Shader::create: metal_source after shift_metal_graphics_buffer_indices:\n%s", metal_source.c_str());
    if (resolved_entry_point != requested_entry_point) {
        LOGT_DEBUG(
            LogRenderer,
            "Resolved Metal shader entry point [%s:%s] -> [%s]",
            debug_name.c_str(),
            requested_entry_point.c_str(),
            resolved_entry_point.c_str()
        );
    }

    shader_desc.entryPoint = resolved_entry_point.c_str();
    shader_desc.source = metal_source.c_str();
    shader_desc.sourceSize = metal_source.size();
    shader_desc.sourceType = LLGL::ShaderSourceType::CodeString;
    shader_desc.profile = "2.4";
#endif
    
    if (shader_desc.type == LLGL::ShaderType::Vertex && info.vertex_attribs) {
        shader_desc.vertex = *info.vertex_attribs;
    }

    LLGL::Shader* shader = g_device->CreateShader(shader_desc);
    if (!shader) {
        LOGT_ERROR(LogRenderer, "LLGL returned null for shader [%s:%s]",
                   debug_name.c_str(), resolved_entry_point.c_str());
        return;
    }

    if (const LLGL::Report* report = shader->GetReport()) {
        if (report->HasErrors()) {
            LOGT_ERROR(LogRenderer, "Shader [%s:%s] failed:\n%s",
                       debug_name.c_str(), resolved_entry_point.c_str(), report->GetText());
            g_device->Release(*shader);
            return;
        }
    }

    handle_ = shader;
}

} // namespace fem
