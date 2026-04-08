cmake_minimum_required(VERSION 3.27)
project(DelaunayForge LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CGAL_DO_NOT_WARN_ABOUT_CMAKE_BUILD_TYPE TRUE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Options)
include(Dependencies)
include(Sources)
include(Shaders)

fem_collect_sources(FEM_SOURCES FEM_OPENGL_BACKEND)

add_executable(DelaunayForge ${FEM_SOURCES})
set_target_properties(DelaunayForge PROPERTIES OUTPUT_NAME "DelaunayForge")

fem_target_common_includes(DelaunayForge)
fem_configure_target(DelaunayForge)

if(USE_LLGL)
  target_compile_definitions(DelaunayForge PRIVATE USE_LLGL)
  target_link_libraries(DelaunayForge
    PRIVATE
      glfw
      glm
      LLGL
      LLGL_Null
      LLGL_OpenGL
  )

  if(WIN32)
    target_link_libraries(DelaunayForge
      PRIVATE
        LLGL_Direct3D11
        LLGL_Direct3D12
    )
  elseif(APPLE)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)
    find_library(METAL_FRAMEWORK Metal)
    target_link_libraries(DelaunayForge
      PRIVATE
        LLGL_Metal
        ${QUARTZCORE_FRAMEWORK}
        ${METAL_FRAMEWORK}
    )
  endif()
else()
  target_link_libraries(DelaunayForge PRIVATE glfw glm)
endif()

if(NOT USE_BGFX AND NOT USE_LLGL)
  target_sources(DelaunayForge PRIVATE ${FEM_OPENGL_BACKEND})
  target_compile_definitions(DelaunayForge PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
  target_link_libraries(DelaunayForge PRIVATE OpenGL::GL)
endif()

fem_configure_shaders(DelaunayForge)
fem_configure_apple_target(DelaunayForge)
fem_disable_debug_sanitizers_for_third_party()