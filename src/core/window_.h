#ifndef FEM_WINDOW_H
#define FEM_WINDOW_H

#include <glm/vec2.hpp>
#include <string>

struct GLFWwindow;

#if defined(USE_BGFX)

namespace bgfx {
    struct PlatformData;
}

#endif 

namespace fem {

struct WindowInitInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    std::string title = "NoName";
};

class Window {
public:
    bool init(const WindowInitInfo& init_info);
    void shutdown();

#if defined(USE_BGFX)
    void fill_platform_data(bgfx::PlatformData& out_data) const;
#endif

    bool should_close() const;
    void poll_events() const;
    void set_close_window(bool close);

    glm::uvec2 window_size() const;
    glm::ivec2 window_pos() const;
    glm::uvec2 frame_buffer_size() const;

    GLFWwindow* window() const { return window_; }

private:
    GLFWwindow* window_ = nullptr;
};

}

#endif // FEM_WINDOW_H