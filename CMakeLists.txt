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
        WASM_RT_USE_MMAP=0
        WASM_RT_MEMCHECK_BOUNDS_CHECK=1
        PG_UNCHECKED_MEMORY=1
        PG_LOCAL_MEMORY_BASE=1
        WASM_RT_SKIP_SIGNAL_RECOVERY=1
        WASM_RT_MAX_CALL_STACK_DEPTH=10000
    )
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Generated
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Tools/wabt-1.0.42/include
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Tools/wabt-1.0.42/share/wabt/wasm2c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Tools/simde
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Tools/opus-1.5.2/include
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Vendor/blink
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Vendor/zstd
)

# Source files
set(WINDOWS_SOURCES
    windows/WinMain.cpp
    windows/WinGraphicsHost.cpp
    windows/WinInput.cpp
    windows/WinAudio.cpp
    windows/WinFiles.cpp
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
foreach(SRC ${WABT_RUNTIME})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SRC})
        list(APPEND ENGINE_SOURCES ${SRC})
    endif()
endforeach()

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
    )
endif()

# MSVC specific flags
if(MSVC)
    target_compile_options(GTA5Port PRIVATE
        /W3
        /permissive-
        /EHsc
        /fp:fast
        /wd4996
        /wd4005
        /wd4244
        /wd4267
        /FI"${CMAKE_CURRENT_SOURCE_DIR}/windows/compat/msvc_compat.h"
    )
endif()

message(STATUS "Configured GTA5Port native Direct3D 11 Windows build.")
