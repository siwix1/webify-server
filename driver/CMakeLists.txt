cmake_minimum_required(VERSION 3.20)
project(WebifyVDD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Paths
set(WDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
set(WDK_VERSION "10.0.26100.0")
set(UMDF_VERSION "2.33")
set(IDDCX_VERSION "1.4")

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64" OR CMAKE_GENERATOR_PLATFORM STREQUAL "arm64")
    set(WDK_ARCH "arm64")
else()
    set(WDK_ARCH "x64")
endif()

# Build as DLL (UMDF driver)
add_library(WebifyVDD SHARED Driver.cpp)

# Output as .dll
set_target_properties(WebifyVDD PROPERTIES
    OUTPUT_NAME "WebifyVDD"
    SUFFIX ".dll"
)

# Preprocessor defines for UMDF driver
target_compile_definitions(WebifyVDD PRIVATE
    NOMINMAX
    _WINDOWS
    UNICODE
    _UNICODE
    UMDF_USING_NTSTATUS
    UMDF_VERSION_MAJOR=2
    UMDF_VERSION_MINOR=33
    UMDF_MINIMUM_VERSION_REQUIRED=33
    IDDCX_VERSION_MAJOR=1
    IDDCX_VERSION_MINOR=4
    IDDCX_MINIMUM_VERSION_REQUIRED=4
)

# Include directories
target_include_directories(WebifyVDD PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${WDK_ROOT}/Include/${WDK_VERSION}/shared"
    "${WDK_ROOT}/Include/${WDK_VERSION}/um"
    "${WDK_ROOT}/Include/${WDK_VERSION}/km"
    "${WDK_ROOT}/Include/${WDK_VERSION}/ucrt"
    "${WDK_ROOT}/Include/wdf/umdf/${UMDF_VERSION}"
    "${WDK_ROOT}/Include/${WDK_VERSION}/um/iddcx/${IDDCX_VERSION}"
)

# Library directories
target_link_directories(WebifyVDD PRIVATE
    "${WDK_ROOT}/Lib/${WDK_VERSION}/um/${WDK_ARCH}"
    "${WDK_ROOT}/Lib/${WDK_VERSION}/ucrt/${WDK_ARCH}"
    "${WDK_ROOT}/Lib/${WDK_VERSION}/um/${WDK_ARCH}/iddcx/${IDDCX_VERSION}"
    "${WDK_ROOT}/Lib/wdf/umdf/${WDK_ARCH}/${UMDF_VERSION}"
)

# Link libraries
target_link_libraries(WebifyVDD PRIVATE
    WdfDriverStubUm
    iddcxstub
    d3d11
    dxgi
    avrt
    ole32
    ucrt
    ntdll
    onecoreuap
)

# Disable WPP tracing for now (the .tmh files need WPP preprocessor)
# Instead, we'll strip the tracing includes
target_compile_definitions(WebifyVDD PRIVATE WPP_INIT_TRACING=__noop WPP_CLEANUP=__noop)
