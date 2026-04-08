#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace fem {

struct OrbitCamera {
    float yaw_deg   = -35.f;
    float pitch_deg =  25.f;
    float dist      = 800.f;
    glm::vec3 target{0.f, 0.f, 0.f}; // X right, Y up, Z forward

    void clamp() {
        if (pitch_deg < -85.f) pitch_deg = -85.f;
        if (pitch_deg >  85.f) pitch_deg =  85.f;
        if (dist < 1.f)        dist = 1.f;
    }
    glm::vec3 eye() const {
        float cy = std::cos(glm::radians(yaw_deg));
        float sy = std::sin(glm::radians(yaw_deg));
        float cp = std::cos(glm::radians(pitch_deg));
        float sp = std::sin(glm::radians(pitch_deg));
        glm::vec3 dir{ cy*cp, sp, sy*cp };      // look direction
        return target - dist * dir;             // OK: float * vec3
    }
    glm::mat4 view() const {
        return glm::lookAt(eye(), target, glm::vec3(0.f,1.f,0.f));
    }
    glm::mat4 proj(float w, float h) const {
        float aspect = (h > 0.f) ? (w / h) : 1.f;
        float n = std::max(0.01f, dist * 0.05f);
        float f = std::max(n + 10.f, dist * 10.f);
        return glm::perspective(glm::radians(45.f), aspect, n, f);
    }
};

}