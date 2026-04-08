include_guard(GLOBAL)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(USE_BGFX "Render with bgfx instead of raw OpenGL" ON)
option(USE_LLGL "Render with LLGL instead of bgfx" OFF)

if(USE_LLGL AND USE_BGFX)
  message(STATUS "USE_LLGL=ON forcing USE_BGFX=OFF")
  set(USE_BGFX OFF CACHE BOOL "Render with bgfx instead of raw OpenGL" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(
    "$<$<CONFIG:Release>:-O3>"
    "$<$<CONFIG:Release>:-march=native>"
    "$<$<CONFIG:Release>:-ffast-math>"
    "$<$<CONFIG:Release>:-DNDEBUG>"
  )
endif()

function(fem_configure_target target)
  target_compile_definitions(${target} PRIVATE
    NOMINMAX
    _ENABLE_EXTENDED_ALIGNED_STORAGE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  )

  if(MSVC)
    target_compile_options(${target} PRIVATE /Zc:preprocessor)
  endif()
endfunction()

function(fem_configure_apple_target target)
  if(NOT APPLE)
    return()
  endif()

  target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/file_system/file_dialog_mac.mm
  )

  target_link_libraries(${target} PRIVATE
    "-framework Accelerate"
    "-framework AppKit"
    "-framework UniformTypeIdentifiers"
  )
endfunction()

function(fem_disable_debug_sanitizers_for_third_party)
  foreach(tgt IN ITEMS shaderc spirv-cross spirv-opt bimg bx bgfx)
    if(TARGET ${tgt})
      target_compile_options(${tgt} PRIVATE $<$<CONFIG:Debug>:-fno-sanitize=all>)
      target_link_options(${tgt} PRIVATE $<$<CONFIG:Debug>:-fno-sanitize=all>)
    endif()
  endforeach()
endfunction()