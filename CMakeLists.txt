cmake_minimum_required(VERSION 3.20)
project(GTA5Port LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows x64 Native Definitions
if(WIN32)
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
        _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
        WASM_RT_USE_MMAP=0
        WASM_RT_MEMCHECK_BOUNDS_CHECK=1
        PG_UNCHECKED_MEMORY=1
        PG_LOCAL_MEMORY_BASE=1
        WASM_RT_SKIP_SIGNAL_RECOVERY=1
        WASM_RT_MAX_CALL_STACK_DEPTH=10000
    )
endif()

# Check if AOT compiled game engine wasm2c output exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app/Generated/game.h")
    set(PG_HAS_GENERATED_GAME 1)
    add_compile_definitions(PG_HAS_GENERATED_GAME=1)
    message(STATUS "AOT Game Engine: FOUND in app/Generated (game.h)")
else()
    set(PG_HAS_GENERATED_GAME 0)
    add_compile_definitions(PG_HAS_GENERATED_GAME=0)
    message(STATUS "AOT Game Engine: NOT found (using stub engine shell)")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Generated
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Tools/wabt-1.0.42/include
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Tools/wabt-1.0.42/share/wabt/wasm2c
    ${CMAKE_CURRENT_SOURCE_DIR}/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/third_party/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Tools/simde
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Tools/opus-1.5.2/include
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Vendor/blink
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Vendor/zstd
)

# Static MSVC C/C++ Runtime Library (/MT) so no VC++ Redistributable is required on Windows
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Source files
set(WINDOWS_SOURCES
    windows/WinMain.cpp
    windows/WinGraphicsHost.cpp
    windows/WinInput.cpp
    windows/WinAudio.cpp
    windows/WinFiles.cpp
    windows/WinUI.cpp
    windows/third_party/imgui/imgui.cpp
    windows/third_party/imgui/imgui_draw.cpp
    windows/third_party/imgui/imgui_tables.cpp
    windows/third_party/imgui/imgui_widgets.cpp
    windows/third_party/imgui/backends/imgui_impl_win32.cpp
    windows/third_party/imgui/backends/imgui_impl_dx11.cpp
)

set(ENGINE_SOURCES
    app/Source/EngineHost.c
    app/Source/NativeASI.c
    app/Source/NativeFiles.c
    app/Source/NativeThreads.c
    app/Source/NativeDecompress.c
    app/Source/NativeHudLayout.c
    app/Source/NativeMeshRelease.c
    app/Source/NativeUserMusic.c
    app/Source/WasmFutex.c
    app/Source/BC1ToASTC.c
    app/Source/BCTextureDecoder.c
    app/Source/RenderListGuard.c
    app/Source/StoredVehicleMods.c
    app/Source/NativeStreamingHealth.c
    app/Source/NativeThreadCPU.c
    app/Source/NativeMemoryBreakdown.c
    app/Source/NativeProxy.c
    windows/compat/wasm-rt-stubs.c
)

# Check if AOT compiled game engine wasm2c output exists
file(GLOB GENERATED_ENGINE_SOURCES "app/Generated/*.c")
if(GENERATED_ENGINE_SOURCES)
    list(APPEND ENGINE_SOURCES ${GENERATED_ENGINE_SOURCES})
    message(STATUS "Including pre-generated AOT engine source files.")
endif()

# Check for wabt runtime implementations
set(WABT_RUNTIME
    app/Tools/wabt-1.0.42/share/wabt/wasm2c/wasm-rt-impl.c
    app/Tools/wabt-1.0.42/share/wabt/wasm2c/wasm-rt-mem-impl.c
    app/Tools/wabt-1.0.42/share/wabt/wasm2c/wasm-rt-exceptions-impl.c
)
set(HAS_WABT_RUNTIME FALSE)
foreach(SRC ${WABT_RUNTIME})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SRC})
        list(APPEND ENGINE_SOURCES ${SRC})
        set(HAS_WABT_RUNTIME TRUE)
    endif()
endforeach()
if(HAS_WABT_RUNTIME)
    add_compile_definitions(PG_HAS_WABT_RUNTIME=1)
    message(STATUS "WABT runtime implementation: FOUND")
else()
    add_compile_definitions(PG_HAS_WABT_RUNTIME=0)
    message(STATUS "WABT runtime implementation: NOT found (using stubs)")
endif()

# Add executable
add_executable(GTA5Port WIN32
    ${WINDOWS_SOURCES}
    ${ENGINE_SOURCES}
)

# Link Libraries
if(WIN32)
    target_link_libraries(GTA5Port PRIVATE
        d3d11
        dxgi
        d3dcompiler
        xinput
        ole32
        shell32
        shlwapi
        uuid
        user32
        gdi32
        advapi32
        winmm
        avrt
        synchronization
        ws2_32
        dwmapi
        imm32
    )
endif()

# MSVC specific flags & Static Runtime
if(MSVC)
    set_property(TARGET GTA5Port PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(GTA5Port PRIVATE
        /W3
        /permissive-
        /EHsc
        /fp:fast
        /wd4996
        /wd4005
        /wd4244
        /wd4267
        /FImsvc_compat.h
        $<$<AND:$<C_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C>>:/experimental:c11atomics>
    )
endif()

message(STATUS "Configured GTA5Port native Direct3D 11 Windows build.")
