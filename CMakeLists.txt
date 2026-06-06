cmake_minimum_required(VERSION 3.27)

if(WIN32)
  project(DelaunayForge LANGUAGES C CXX)
elseif(APPLE)
  project(DelaunayForge LANGUAGES C CXX OBJC OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CGAL_DO_NOT_WARN_ABOUT_CMAKE_BUILD_TYPE TRUE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Options)
include(Dependencies)
include(Sources)
include(Shaders)

fem_collect_sources(FEM_SOURCES)

add_executable(DelaunayForge ${FEM_SOURCES})
set_target_properties(DelaunayForge PROPERTIES OUTPUT_NAME "DelaunayForge")

fem_target_common_includes(DelaunayForge)
fem_configure_target(DelaunayForge)

target_link_libraries(DelaunayForge
  PRIVATE
    glfw
    glm
    LLGL
    LLGL_Null
)

if(WIN32)
  target_link_libraries(DelaunayForge
    PRIVATE
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

fem_configure_shaders(DelaunayForge)
fem_configure_apple_target(DelaunayForge)
fem_disable_debug_sanitizers_for_third_party()