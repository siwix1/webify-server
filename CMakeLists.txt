cmake_minimum_required(VERSION 3.20)
project(webify-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific libraries
if(WIN32)
    set(WIN32_LIBS
        user32
        gdi32
        dwmapi
        d3d11
        dxgi
        ws2_32
        mfplat
        mfuuid
        mf
        evr
        strmiids
    )
endif()

# Core library (desktop management, capture, encoding)
add_library(webify-core STATIC
    src/desktop_manager.cpp
    src/screen_capture.cpp
    src/input_handler.cpp
    src/encoder.cpp
    src/signaling_server.cpp
)

target_include_directories(webify-core PUBLIC include)

if(WIN32)
    target_link_libraries(webify-core PUBLIC ${WIN32_LIBS})
endif()

# Main executable
add_executable(webify-server src/main.cpp)
target_link_libraries(webify-server PRIVATE webify-core)

# Simple test for desktop creation (run on Windows)
add_executable(test-desktop test/test_desktop.cpp)
target_link_libraries(test-desktop PRIVATE webify-core)
