cmake_minimum_required(VERSION 3.20)
project(SubtitleSynchAC1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# force 32-bit build
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Build must be x86 (32-bit), not x64.")
endif()

set(ROOT ${CMAKE_SOURCE_DIR})

add_library(SubtitleSynchAC1 SHARED
    src/main.cpp
    src/mod_runtime.cpp
    src/subtitle_runtime.cpp
    src/subtitle_core.cpp
    src/audio_system.cpp
    src/overlay_ui.cpp
    src/globals.cpp
    src/console.cpp
    src/hooks/asm_hooks.cpp
    src/hooks/dx9_hook.cpp
    src/hooks/dx10_hook.cpp
    src/ini_config.cpp
    src/pattern_scan.cpp

    external/patcher/patcher.cpp
)

add_library(minhook STATIC
    external/minhook/src/hook.c
    external/minhook/src/buffer.c
    external/minhook/src/trampoline.c
    external/minhook/src/hde/hde32.c
    external/minhook/src/hde/hde64.c
)

target_include_directories(minhook PUBLIC
    external/minhook/include
    external/minhook/src/hde
)

target_compile_definitions(minhook PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_compile_options(SubtitleSynchAC1 PRIVATE -msse -msse2)

set(IMGUI_DIR ${ROOT}/external/imgui)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx9.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx10.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMGUI_DIR}/misc/cpp

)

target_compile_definitions(imgui PUBLIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_include_directories(SubtitleSynchAC1 PRIVATE
    ${ROOT}/src
    ${ROOT}/include
    ${ROOT}/external/imgui
    ${ROOT}/external/imgui/backends
    ${ROOT}/external/patcher
    ${ROOT}/external/xbyak
)

target_compile_definitions(SubtitleSynchAC1 PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    $<$<CONFIG:Debug>:ENABLE_CONSOLE>
)

if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static")
endif()

target_link_libraries(SubtitleSynchAC1 PRIVATE
    minhook
    imgui
    d3d9
    d3d10
    d3dcompiler
    dxgi
    dwmapi
)

set_target_properties(SubtitleSynchAC1 PROPERTIES
    PREFIX ""
    SUFFIX ".asi"
    OUTPUT_NAME "SubtitleSynchAC1"
)

add_custom_command(TARGET SubtitleSynchAC1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${ROOT}/assets
        $<TARGET_FILE_DIR:SubtitleSynchAC1>
)