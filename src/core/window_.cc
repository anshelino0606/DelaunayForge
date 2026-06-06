#include "window_.h"
#include "keyboard.h"
#include "mouse.h"
#include "log_categories.h"

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

bool Window::init(const WindowInitInfo& init_info) {
    if (!glfwInit()) { 
        LOGT_ERROR(LogApplication, "Failed to initialize GLFW");
        return false; 
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GL_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    LOGT_DEBUG(LogApplication, "glfwCreateWindow()");

    window_ = glfwCreateWindow(
        init_info.width, 
        init_info.height,
        init_info.title.c_str(),
        nullptr, 
        nullptr
    );

    if (!window_) { 
        LOGT_ERROR(LogApplication, "Failed to create GLFW window");
        glfwTerminate(); 
        return false; 
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, Keyboard::keyCallback);
    glfwSetCursorPosCallback(window_, Mouse::cursorPosCallback);
    glfwSetMouseButtonCallback(window_, Mouse::mouseButtonCallback);
    glfwSetScrollCallback(window_, Mouse::mouseWheelCallback);

    return true;
}

void Window::shutdown() {
    if (window_) {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}

bool Window::should_close() const {
    if (window_) {
        return glfwWindowShouldClose(window_);
    }
    return false;
}

void Window::poll_events() const {
    glfwPollEvents();
}

void Window::set_close_window(bool close) {
    glfwSetWindowShouldClose(window_, close ? GLFW_TRUE : GLFW_FALSE);
}

glm::uvec2 Window::window_size() const {
    if (window_) {
        int width = 0, height = 0;
        glfwGetWindowSize(window_, &width, &height);
        return { width, height };
    }

    return { 0, 0 };
}

glm::ivec2 Window::window_pos() const {
    if (window_) {
        int width = 0, height = 0;
        glfwGetWindowPos(window_, &width, &height);
        return { width, height };
    }

    return { 0, 0 };
}

glm::uvec2 Window::frame_buffer_size() const {
    if (window_) {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        return { width, height };
    }
    
    return { 0, 0 };
}

}