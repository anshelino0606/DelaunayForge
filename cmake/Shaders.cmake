include_guard(GLOBAL)

function(fem_configure_shaders target)
  message(STATUS "Building with LLGL - note: shader sources should be in GLSL/HLSL format")
  set(shader_dir ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
  target_compile_definitions(${target} PRIVATE SHADER_DIR="${shader_dir}")
endfunction()