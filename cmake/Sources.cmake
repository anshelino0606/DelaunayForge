include_guard(GLOBAL)

function(fem_collect_sources out_sources out_opengl_backend)
  file(GLOB_RECURSE project_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
  )

  file(GLOB root_headers CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
  )

  file(GLOB imgui_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/misc/cpp/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/misc/cpp/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/backends/imgui_impl_glfw.h
  )

  if(WIN32)
    file(GLOB imgui_backend CONFIGURE_DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/backends/imgui_impl_dx12.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/backends/imgui_impl_dx12.h
    )
  elseif(APPLE)
    file(GLOB imgui_backend CONFIGURE_DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/backends/imgui_impl_metal.mm
      ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/backends/imgui_impl_metal.h
    )
  else()
    set(imgui_backend)
  endif()

  file(GLOB imgui_opengl_backend CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/backends/imgui_impl_opengl3.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/glad/*.c
  )

  file(GLOB lz4_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lz4/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lz4/*.h
  )

  list(APPEND project_sources ${root_headers} ${imgui_sources} ${imgui_backend} ${lz4_sources} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

  list(FILTER project_sources EXCLUDE REGEX ".*/gui/delaunay_gui_ref\\.cc$")
  list(FILTER project_sources EXCLUDE REGEX ".*/opengl/.*")
  list(FILTER project_sources EXCLUDE REGEX ".*/(shader|vao|vbo|ebo|ubo|mesh)\\.(cc|cpp)$")

  if(NOT USE_LLGL)
    list(FILTER project_sources EXCLUDE REGEX ".*/renderer/(surface|imgui_renderer)\\.(cc|cpp)$")
  endif()

  set(${out_sources} ${project_sources} PARENT_SCOPE)
  set(${out_opengl_backend} ${imgui_opengl_backend} PARENT_SCOPE)
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
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/backends
  )
endfunction()