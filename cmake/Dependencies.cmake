include_guard(GLOBAL)

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(ENV{GIT_TERMINAL_PROMPT} 0)

if(NOT USE_BGFX AND NOT USE_LLGL)
  find_package(OpenGL REQUIRED)
endif()

if(DEFINED BGFX_CMAKE_DIR AND EXISTS "${BGFX_CMAKE_DIR}/CMakeLists.txt")
  message(STATUS "Using local bgfx.cmake at: ${BGFX_CMAKE_DIR}")
  set(FETCHCONTENT_SOURCE_DIR_BGFX_CMAKE "${BGFX_CMAKE_DIR}")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LLGL/CMakeLists.txt")
  message(STATUS "Using local LLGL at: ${CMAKE_CURRENT_SOURCE_DIR}/LLGL")
  set(FETCHCONTENT_SOURCE_DIR_LLGL "${CMAKE_CURRENT_SOURCE_DIR}/LLGL")
endif()

if(USE_BGFX)
  set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)
  set(BGFX_USE_OVR OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    bgfx_cmake
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG master
  )
  FetchContent_MakeAvailable(bgfx_cmake)

  if(APPLE)
    foreach(bgfx_tgt IN ITEMS bgfx bgfx-shared)
      if(TARGET ${bgfx_tgt})
        target_compile_definitions(${bgfx_tgt} PRIVATE BGFX_CONFIG_RENDERER_WEBGPU=0)
      endif()
    endforeach()
  endif()
endif()

if(USE_LLGL)
  set(LLGL_ENABLE_DIRECT3D12 ON CACHE BOOL "" FORCE)
  set(LLGL_ENABLE_DIRECT3D11 OFF CACHE BOOL "" FORCE)
  set(LLGL_ENABLE_VULKAN ON CACHE BOOL "" FORCE)
  set(LLGL_ENABLE_METAL ON CACHE BOOL "" FORCE)
  set(LLGL_ENABLE_OPENGL ON CACHE BOOL "" FORCE)
  set(LLGL_ENABLE_OPENGLES OFF CACHE BOOL "" FORCE)
  set(LLGL_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(LLGL_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(LLGL_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
  set(LLGL_ENABLE_DEBUG_LAYER ON CACHE BOOL "" FORCE)

  FetchContent_Declare(
    LLGL
    GIT_REPOSITORY https://github.com/LukasBanana/LLGL.git
    GIT_TAG master
  )
  FetchContent_MakeAvailable(LLGL)
endif()

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.2
)
FetchContent_MakeAvailable(glm)