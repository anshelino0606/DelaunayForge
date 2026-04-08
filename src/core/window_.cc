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

#if defined(USE_BGFX)
#include <bgfx/bgfx.h>
#endif 

namespace fem {

bool Window::init(const WindowInitInfo& init_info) {
    if (!glfwInit()) { 
        LOGT_ERROR(LogApplication, "Failed to initialize GLFW");
        return false; 
    }

#if !defined(USE_BGFX) && !defined(USE_LLGL)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#else
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

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

#if !defined(USE_BGFX) && !defined(USE_LLGL)
    glfwMakeContextCurrent(window_);
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow*,int w,int h){ glViewport(0,0,w,h); });
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOGT_WARN(LogApplication, "Failed to initialize GLAD\n"); return false;
    }
    glfwSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
#endif

    glfwSetKeyCallback(window_, Keyboard::keyCallback);
    glfwSetCursorPosCallback(window_, Mouse::cursorPosCallback);
    glfwSetMouseButtonCallback(window_, Mouse::mouseButtonCallback);
    glfwSetScrollCallback(window_, Mouse::mouseWheelCallback);

    return true;
}

#if defined(USE_BGFX)
void Window::fill_platform_data(bgfx::PlatformData& out_data) const {
#if defined(_WIN32)
    out_data.nwh = glfwGetWin32Window(window_);
    out_data.ndt = nullptr;
#elif defined(__APPLE__)
    out_data.nwh = glfwGetCocoaWindow(window_);
    out_data.ndt = nullptr;
#elif defined(GLFW_EXPOSE_NATIVE_WAYLAND)
    out_data.nwh = (void*)glfwGetWaylandWindow(window_);
    out_data.ndt = glfwGetWaylandDisplay();
#else
    out_data.nwh = (void*)(uintptr_t)glfwGetX11Window(window_);
    out_data.ndt = glfwGetX11Display();
#endif
    out_data.context      = nullptr;
    out_data.backBuffer   = nullptr;
    out_data.backBufferDS = nullptr;
}
#endif

void Window::shutdown() {
    if (window_) {
#ifndef USE_BGFX
        glfwDestroyWindow(window_);
        glfwTerminate();
#else
        glfwDestroyWindow(window_);
        glfwTerminate();
#endif
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