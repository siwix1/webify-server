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
        windowscodecs
        ole32
    )
endif()

# Core library (desktop management, capture, encoding)
add_library(webify-core STATIC
    src/desktop_manager.cpp
    src/screen_capture.cpp
    src/input_handler.cpp
    src/encoder.cpp
    src/signaling_server.cpp
    src/ws_server.cpp
    src/jpeg_encoder.cpp
    src/vdd_capture.cpp
)

target_include_directories(webify-core PUBLIC include src)
target_compile_definitions(webify-core PUBLIC NOMINMAX)

if(WIN32)
    target_link_libraries(webify-core PUBLIC ${WIN32_LIBS})
endif()

# Main executable
add_executable(webify-server src/main.cpp)
target_link_libraries(webify-server PRIVATE webify-core)

# Simple test for desktop creation (run on Windows)
add_executable(test-desktop test/test_desktop.cpp)
target_link_libraries(test-desktop PRIVATE webify-core)

# Test window app — simple Win32 window for capture testing
add_executable(test-window WIN32 test/test_window.cpp)
if(WIN32)
    target_link_libraries(test-window PRIVATE user32 gdi32)
endif()

# Demo app — Win32 app with text input for multi-session testing
add_executable(demo-app WIN32 test/demo_app.cpp)
if(WIN32)
    target_link_libraries(demo-app PRIVATE user32 gdi32)
endif()
