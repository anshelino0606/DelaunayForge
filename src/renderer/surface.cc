#include "surface.h"
#include "core/window_.h"
#include <LLGL/Platform/NativeHandle.h>

#if defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
    #define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
    #if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
        #define GLFW_EXPOSE_NATIVE_WAYLAND
    #else
        #define GLFW_EXPOSE_NATIVE_X11
    #endif
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace fem {

Surface::Surface(Window* window) : window_(window) {

}

bool Surface::GetNativeHandle(void* nativeHandle, std::size_t nativeHandleSize) {
    if (nativeHandle != nullptr && nativeHandleSize == sizeof(LLGL::NativeHandle)) {
        GLFWwindow* glfw_window = window_->window();
        LLGL::NativeHandle* handle = reinterpret_cast<LLGL::NativeHandle*>(nativeHandle);

        #if defined(_WIN32)
            handle->window = glfwGetWin32Window(glfw_window);
        #elif defined(__APPLE__)
            handle->responder = glfwGetCocoaWindow(glfw_window);
        #elif defined(GLFW_EXPOSE_NATIVE_WAYLAND)
            handle->window = (void*)glfwGetWaylandWindow(glfw_window);
        #else
            handle->window = (void*)(uintptr_t)glfwGetX11Window(glfw_window);
        #endif

        return true;
    }
    return false;
}

LLGL::Extent2D Surface::GetContentSize() const {
    glm::uvec2 size = window_->frame_buffer_size();
    return { size.x, size.y };
}

bool Surface::AdaptForVideoMode(LLGL::Extent2D* resolution, bool* fullscreen) {
    // TODO
    return true;
}

LLGL::Display* Surface::FindResidentDisplay() const {
    glm::ivec2 window_pos_vec = window_->window_pos();
    glm::uvec2 window_size_vec = window_->window_size();

    const LLGL::Offset2D win_pos = { window_pos_vec.x, window_pos_vec.y };
    const LLGL::Extent2D win_size = { window_size_vec.x, window_size_vec.y };
    const int win_area = static_cast<int>(win_size.width * win_size.height);

    for (auto displays = LLGL::Display::GetList(); LLGL::Display* display = *displays; ++displays)
    {
        LLGL::Offset2D offset = display->GetOffset();
        LLGL::Extent2D extent = display->GetDisplayMode().resolution;

        int scrX = static_cast<int>(extent.width);
        int scrY = static_cast<int>(extent.height);

        /* Calculate window boundaries relative to the current display */
        int x1 = win_pos.x - offset.x;
        int y1 = win_pos.y - offset.y;
        int x2 = x1 + static_cast<int>(win_size.width);
        int y2 = y1 + static_cast<int>(win_size.height);

        /* Is window fully or partially inside the dispaly? */
        if (x2 >= 0 && x1 <= scrX &&
            y2 >= 0 && y1 <= scrY)
        {
            /* Is at least the half of the window inside the display? */
            x1 = std::max(0, x1);
            y1 = std::max(0, y1);

            x2 = std::min(x2 - x1, scrX);
            y2 = std::min(y2 - y1, scrY);

            auto visArea = x2 * y2;

            if (visArea * 2 >= win_area)
                return display;
        }
    }

    return nullptr;
}

}