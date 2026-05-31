cmake_minimum_required(VERSION 3.16)
project(SdrSdk VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
include(GNUInstallDirs)

option(SDRSDK_BUILD_EXAMPLES "Build example programs" ON)
option(SDRSDK_BUILD_TESTS    "Build unit tests"       ON)

# ── SdrTaskApi — shared types and MessageCodec ────────────────────────────────
# Look for it as a sibling first (developer layout), else fetch from GitHub.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../SdrTaskApi/CMakeLists.txt")
    message(STATUS "SdrSdk: using sibling SdrTaskApi")
    set(_taskapi_src "${CMAKE_CURRENT_SOURCE_DIR}/../SdrTaskApi")
    add_subdirectory(../SdrTaskApi ${CMAKE_BINARY_DIR}/SdrTaskApi)
else()
    include(FetchContent)
    FetchContent_Declare(SdrTaskApi
        GIT_REPOSITORY https://github.com/BMichaud7/SdrTaskApi.git
        GIT_TAG        main/1.0
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SdrTaskApi)
    set(_taskapi_src "${sdrtaskapi_SOURCE_DIR}")
endif()

# ── Au units library (header-only) ───────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(Au
    GIT_REPOSITORY https://github.com/aurora-opensource/au
    GIT_TAG        "0.5.1"
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(Au)

# ── FFTW3 ─────────────────────────────────────────────────────────────────────
# On Windows (vcpkg): FFTW3 ships a CMake config package with imported targets.
# On Linux: use pkg-config with IMPORTED_TARGET so PkgConfig::FFTW3F is a
# proper target that survives cmake install export.
if(WIN32)
    find_package(FFTW3 CONFIG REQUIRED)
    set(FFTW3_TARGET FFTW3::fftw3f)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFTW3F REQUIRED IMPORTED_TARGET fftw3f)
    set(FFTW3_TARGET PkgConfig::FFTW3F)
endif()

# ── Proton ────────────────────────────────────────────────────────────────────
# pkg-config for proton is broken on Ubuntu 24.04 multiarch and absent on
# Windows. Use find_path/find_library and wrap in an IMPORTED target so the
# install export embeds a proper target rather than absolute paths.
find_path(PROTON_INCLUDE_DIRS proton/connection.hpp REQUIRED)
find_library(PROTON_CPP_LIB NAMES qpid-proton-cpp REQUIRED)
find_library(PROTON_C_LIB   NAMES qpid-proton     REQUIRED)

add_library(SdrSdk_Proton UNKNOWN IMPORTED)
set_target_properties(SdrSdk_Proton PROPERTIES
    IMPORTED_LOCATION             "${PROTON_CPP_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${PROTON_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES      "${PROTON_C_LIB}"
)

# ── Library ───────────────────────────────────────────────────────────────────
add_library(sdrsdk
    src/SdrClient.cpp
    src/IqStream.cpp
    src/Spectrum.cpp
)
add_library(SdrSdk::sdrsdk ALIAS sdrsdk)

target_include_directories(sdrsdk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${_taskapi_src}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(sdrsdk
    PUBLIC  ${FFTW3_TARGET} SdrSdk_Proton Au::au
    PRIVATE sdr_task_api
)
# Install SdrTaskApi headers as part of SdrSdk so consumers get <sdr/Types.hpp>
install(DIRECTORY "${_taskapi_src}/include/sdr" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ── Examples ──────────────────────────────────────────────────────────────────
if(SDRSDK_BUILD_EXAMPLES)
    foreach(ex 01_narrowband 02_scan_and_detect 03_triggered_capture)
        add_executable(${ex} examples/${ex}.cpp)
        target_link_libraries(${ex} PRIVATE sdrsdk)
    endforeach()
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
if(SDRSDK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Install (only when SdrSdk is the top-level project) ──────────────────────
# cmake EXPORT targets are omitted: FetchContent deps (nlohmann_json, spdlog)
# pulled in by SdrTaskApi would also need to be in the export set, which is
# impractical. SdrSdk is always consumed via add_subdirectory() in practice.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(TARGETS sdrsdk sdr_task_api
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
