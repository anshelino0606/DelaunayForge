include_guard(GLOBAL)

function(fem_compile_bgfx_shader target shader_path shader_type varying_path shader_out shader_platform shader_profile shaderc_exe include_core include_common)
  get_filename_component(shader_name ${shader_path} NAME_WE)
  set(shader_source_path ${CMAKE_SOURCE_DIR}/shaders/${shader_path})
  set(shader_output_path ${shader_out}/${shader_name}.bin)
  set(shader_depends ${shader_source_path})

  set(shader_command_args
    -f ${shader_source_path}
    -o ${shader_output_path}
    --type ${shader_type}
    --platform ${shader_platform}
    --profile ${shader_profile}
    -i ${include_core}
    -i ${include_common}
  )

  if(varying_path)
    set(varying_global_path ${CMAKE_SOURCE_DIR}/shaders/${varying_path})
    list(APPEND shader_command_args --varying ${varying_global_path})
    list(APPEND shader_depends ${varying_global_path})
  endif()

  string(MAKE_C_IDENTIFIER "shader_${shader_path}_${shader_type}" shader_target)

  add_custom_command(
    OUTPUT ${shader_output_path}
    COMMAND ${shaderc_exe} ${shader_command_args}
    DEPENDS ${shader_depends}
  )

  add_custom_target(${shader_target} DEPENDS ${shader_output_path})
  add_dependencies(${target} ${shader_target})
endfunction()

function(fem_configure_shaders target)
  if(USE_BGFX)
    set(shader_out ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    file(MAKE_DIRECTORY ${shader_out})

    target_compile_definitions(${target} PRIVATE USE_BGFX SHADER_DIR="${shader_out}")
    target_link_libraries(${target} PRIVATE bgfx bx bimg)

    if(APPLE)
      find_library(COCOA_FRAMEWORK Cocoa)
      find_library(IOKIT_FRAMEWORK IOKit)
      find_library(METAL_FRAMEWORK Metal)
      find_library(QUARTZCORE_FRAMEWORK QuartzCore)
      find_library(METALKIT_FRAMEWORK MetalKit)
      target_link_libraries(${target} PRIVATE
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
      )

      set(shader_platform osx)
      set(shader_profile metal)
    elseif(WIN32)
      set(shader_platform windows)
      set(shader_profile spirv)
    else()
      set(shader_platform linux)
      set(shader_profile spirv)
    endif()

    if(TARGET shaderc)
      set(shaderc_exe $<TARGET_FILE:shaderc>)
    elseif(TARGET bgfx::shaderc)
      set(shaderc_exe $<TARGET_FILE:bgfx::shaderc>)
    else()
      message(FATAL_ERROR "bgfx shaderc tool not found")
    endif()

    set(bgfx_shader_include_core ${bgfx_cmake_SOURCE_DIR}/bgfx/src)
    set(bgfx_shader_include_common ${bgfx_cmake_SOURCE_DIR}/bgfx/examples/runtime/shaders)
    set(varying_def varying.def.sc)
    set(varying_imgui imgui/varying.def.sc)

    fem_compile_bgfx_shader(${target} vs_mesh.sc vertex ${varying_def} ${shader_out} ${shader_platform} ${shader_profile} ${shaderc_exe} ${bgfx_shader_include_core} ${bgfx_shader_include_common})
    fem_compile_bgfx_shader(${target} fs_mesh.sc fragment ${varying_def} ${shader_out} ${shader_platform} ${shader_profile} ${shaderc_exe} ${bgfx_shader_include_core} ${bgfx_shader_include_common})
    fem_compile_bgfx_shader(${target} imgui/vs_imgui_image.sc vertex ${varying_imgui} ${shader_out} ${shader_platform} ${shader_profile} ${shaderc_exe} ${bgfx_shader_include_core} ${bgfx_shader_include_common})
    fem_compile_bgfx_shader(${target} imgui/fs_imgui_image.sc fragment ${varying_imgui} ${shader_out} ${shader_platform} ${shader_profile} ${shaderc_exe} ${bgfx_shader_include_core} ${bgfx_shader_include_common})

    foreach(compute_shader IN ITEMS
      compute/cs_delaunay.sc
      compute/cs_edge_flip.sc
      compute/cs_cavity.sc
      compute/cs_incircle.sc
      compute/cs_refine_sizing.sc
      compute/cs_clip_triangles.sc
      compute/cs_copy_buffer_float.sc
      compute/cs_copy_buffer_uint.sc
      compute/cs_copy_buffer_int.sc
      compute/cs_counter_test.sc
      compute/cs_read_buffer_float.sc
      compute/cs_read_buffer_uint.sc
      compute/cs_update_neighbors.sc
      compute/cs_insert_points.sc
      compute/cs_update_topology.sc
    )
      fem_compile_bgfx_shader(${target} ${compute_shader} compute "" ${shader_out} ${shader_platform} ${shader_profile} ${shaderc_exe} ${bgfx_shader_include_core} ${bgfx_shader_include_common})
    endforeach()
  elseif(USE_LLGL)
    message(STATUS "Building with LLGL - note: shader sources should be in GLSL/HLSL format")
    set(shader_dir ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
    target_compile_definitions(${target} PRIVATE SHADER_DIR="${shader_dir}")
  endif()
endfunction()