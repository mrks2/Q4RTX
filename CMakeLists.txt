cmake_minimum_required(VERSION 3.20)
project(Q4RTX LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Proxy opengl32.dll (32-bit)
# cmake -A Win32 -B build32 && cmake --build build32 --target q4rtx_proxy
add_library(q4rtx_proxy SHARED
    src/proxy/dllmain.cpp
    src/proxy/gl_hooks.cpp
    src/proxy/gl_hooks_state.cpp
    src/proxy/gl_hooks_extensions.cpp
    src/proxy/gl_passthrough.cpp
    src/proxy/frame_sender.cpp
    src/proxy/vertex_extract.cpp
)
target_include_directories(q4rtx_proxy PRIVATE src)
set_target_properties(q4rtx_proxy PROPERTIES
    OUTPUT_NAME "opengl32"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/proxy
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/proxy
)
# We ARE opengl32 - the real one is loaded dynamically at runtime
target_sources(q4rtx_proxy PRIVATE src/proxy/gl_exports.def)

# DXR Renderer (64-bit)
# cmake -A x64 -B build64 && cmake --build build64 --target q4rtx_renderer
add_executable(q4rtx_renderer
    src/renderer/main.cpp
    src/renderer/d3d12_context.cpp
    src/renderer/rt_pipeline.cpp
    src/renderer/scene.cpp
    src/renderer/object.cpp
    src/renderer/light.cpp
    src/renderer/skybox.cpp
    src/renderer/renderer.cpp
    src/renderer/frame_receiver.cpp
    src/renderer/window.cpp
)
target_include_directories(q4rtx_renderer PRIVATE src)
target_link_libraries(q4rtx_renderer PRIVATE d3d12 dxgi dxguid)
set_target_properties(q4rtx_renderer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/renderer
)

# Compile RT shaders with DXC
find_program(DXC_EXE dxc
    HINTS "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
          "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
)
if(DXC_EXE)
    set(SHADER_SRC ${CMAKE_SOURCE_DIR}/shaders/rt_shaders.hlsl)
    set(SHADER_OUT ${CMAKE_SOURCE_DIR}/shaders/rt_shaders.cso)
    add_custom_command(
        OUTPUT ${SHADER_OUT}
        COMMAND ${DXC_EXE} -T lib_6_3 -Fo ${SHADER_OUT} ${SHADER_SRC}
        DEPENDS ${SHADER_SRC}
        COMMENT "Compiling RT shaders with DXC"
    )
    add_custom_target(compile_shaders DEPENDS ${SHADER_OUT})
    add_dependencies(q4rtx_renderer compile_shaders)
else()
    message(WARNING "DXC not found - shaders must be compiled manually")
endif()

# Copy shaders next to renderer
add_custom_command(TARGET q4rtx_renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:q4rtx_renderer>/shaders
)
