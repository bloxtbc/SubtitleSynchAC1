cmake_minimum_required(VERSION 3.20)
project(SubtitleSynchAC1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

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
    src/playback_clock.cpp
    src/pause_manager_probe.cpp
    src/save_activity_monitor.cpp
    src/press_prompt_detector.cpp
    src/audio_resolver.cpp
    src/ambient_voices.cpp
    src/overlay_ui.cpp
    src/subtitle_layout.cpp
    src/globals.cpp
    src/console.cpp
    src/hooks/asm_hooks.cpp
    src/hooks/pause_hooks.cpp
    src/hooks/press_prompt_hook.cpp
    src/hooks/dx9_hook.cpp
    src/hooks/dx10_hook.cpp
    src/ini_config.cpp
    src/diagnostics.cpp
    src/pattern_match.cpp
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

if(MSVC)
    target_compile_options(SubtitleSynchAC1 PRIVATE /arch:SSE2)
else()
    target_compile_options(SubtitleSynchAC1 PRIVATE -msse -msse2)
endif()

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

if(MINGW AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
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

include(CTest)
if(BUILD_TESTING)
    add_executable(pause_tests tests/pause_tests.cpp src/subtitle_runtime.cpp src/audio_system.cpp src/playback_clock.cpp
        src/pause_manager_probe.cpp src/hooks/pause_hooks.cpp src/pattern_scan.cpp src/pattern_match.cpp src/diagnostics.cpp)
    target_include_directories(pause_tests PRIVATE include external/xbyak)
    target_compile_definitions(pause_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(pause_tests PRIVATE minhook)
    add_test(NAME pause_tests COMMAND pause_tests)
    add_executable(layout_tests tests/layout_tests.cpp src/subtitle_layout.cpp)
    target_include_directories(layout_tests PRIVATE include)
    target_link_libraries(layout_tests PRIVATE imgui)
    add_test(NAME layout_tests COMMAND layout_tests)
    add_executable(press_prompt_tests tests/press_prompt_tests.cpp src/press_prompt_detector.cpp
        src/hooks/press_prompt_hook.cpp src/pattern_scan.cpp src/pattern_match.cpp src/diagnostics.cpp)
    target_include_directories(press_prompt_tests PRIVATE include external/xbyak)
    target_compile_definitions(press_prompt_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(press_prompt_tests PRIVATE minhook)
    add_test(NAME press_prompt_tests COMMAND press_prompt_tests)
    add_executable(diagnostics_tests tests/diagnostics_tests.cpp src/diagnostics.cpp)
    target_include_directories(diagnostics_tests PRIVATE include)
    target_compile_definitions(diagnostics_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME diagnostics_tests COMMAND diagnostics_tests)
    add_executable(subtitle_tests tests/subtitle_tests.cpp src/subtitle_core.cpp
        src/subtitle_runtime.cpp src/audio_system.cpp src/audio_resolver.cpp src/ambient_voices.cpp src/pattern_match.cpp)
    target_include_directories(subtitle_tests PRIVATE include)
    add_test(NAME subtitle_tests COMMAND subtitle_tests)
    add_executable(hook_tests tests/hook_tests.cpp src/hooks/asm_hooks.cpp src/playback_clock.cpp
        src/audio_system.cpp src/audio_resolver.cpp src/diagnostics.cpp src/pattern_scan.cpp src/pattern_match.cpp)
    target_include_directories(hook_tests PRIVATE include external/xbyak)
    target_compile_definitions(hook_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(hook_tests PRIVATE minhook)
    add_test(NAME hook_tests COMMAND hook_tests)
endif()
