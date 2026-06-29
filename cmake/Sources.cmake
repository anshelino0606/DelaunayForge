include_guard(GLOBAL)

function(fem_collect_sources out_sources)
  file(GLOB_RECURSE project_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
  )

  file(GLOB root_headers CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/*.hpp
  )

  file(GLOB imgui_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/misc/cpp/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/misc/cpp/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.h
  )

  file(GLOB lz4_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lz4/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lz4/*.h
  )

  list(APPEND project_sources ${root_headers} ${imgui_sources} ${imgui_backend} ${lz4_sources} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

  list(FILTER project_sources EXCLUDE REGEX ".*/gui/delaunay_gui_ref\\.cc$")
  list(FILTER project_sources EXCLUDE REGEX ".*/opengl/.*")
  list(FILTER project_sources EXCLUDE REGEX ".*/(shader|vao|vbo|ebo|ubo|mesh)\\.(cc|cpp)$")

  set(${out_sources} ${project_sources} PARENT_SCOPE)
endfunction()

function(fem_target_common_includes target)
  target_include_directories(${target} PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/application
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/editor
    ${PROJECT_SOURCE_DIR}/src/geom
    ${PROJECT_SOURCE_DIR}/src/logger
    ${PROJECT_SOURCE_DIR}/src/math
    ${PROJECT_SOURCE_DIR}/src/math/fem
    ${PROJECT_SOURCE_DIR}/src/renderer
    ${PROJECT_SOURCE_DIR}/src/rhi
    ${PROJECT_SOURCE_DIR}/src/plot
    ${PROJECT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
  )
endfunction()