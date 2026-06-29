#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <cstdint>

namespace fem {

struct Color8
{
    uint32_t rgba = 0;
    
    Color8(uint32_t rgba) : rgba(rgba) { }
    Color8(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255)
        : rgba((uint32_t)r | (uint32_t)g << 8 | uint32_t(b) << 16 | uint32_t(a) << 24) { }
    
    Color8(const glm::vec3& color) : Color8(
        uint8_t(color.x * 255.0f),
        uint8_t(color.y * 255.0f),
        uint8_t(color.z * 255.0f)) { }
    
    Color8(const glm::vec4& color) : Color8(
        uint8_t(color.x * 255.0f),
        uint8_t(color.y * 255.0f),
        uint8_t(color.z * 255.0f),
        uint8_t(color.w * 255.0f)) { }

    uint8_t get_r() const { return rgba >> 0 & 0xFF; }
    uint8_t get_g() const { return rgba >> 8 & 0xFF; }
    uint8_t get_b() const { return rgba >> 16 & 0xFF; }
    uint8_t get_a() const { return rgba >> 24 & 0xFF; }

    // Returns normalized data from red channel
    float get_nr() const { return static_cast<float>(get_r()) / 255.0f; }
    // Returns normalized data from green channel
    float get_ng() const { return static_cast<float>(get_g()) / 255.0f; }
    // Returns normalized data from blue channel
    float get_nb() const { return static_cast<float>(get_b()) / 255.0f; }
    // Returns normalized data from alpha channel
    float get_na() const { return static_cast<float>(get_a()) / 255.0f; }

    void set_r(uint8_t r) { *this = { r, get_r(), get_b(), get_a() }; }
    void set_g(uint8_t g) { *this = { get_r(), g, get_b(), get_a() }; }
    void set_b(uint8_t b) { *this = { get_r(), get_g(), b, get_a()}; }
    void set_a(uint8_t a) { *this = { get_r(), get_g(), get_b(), a }; }

    glm::vec3 to_vec3() const
    {
        return glm::vec3{ get_nr(), get_ng(), get_nb() };
    }
    
    glm::vec4 to_vec4() const
    {
        return glm::vec4{ get_nr(), get_ng(), get_nb(), get_na() };
    }

    operator glm::vec3() const { return to_vec3(); }
    operator glm::vec4() const { return to_vec4(); }
    operator uint32_t() const { return rgba; }
};

}