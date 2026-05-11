cmake_minimum_required(VERSION 3.16)
project(SdrSdk VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

option(SDRSDK_BUILD_EXAMPLES "Build example programs" ON)
option(SDRSDK_BUILD_TESTS    "Build unit tests"       ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3F REQUIRED fftw3f)

# Proton (pkg-config name differs between distros)
find_path(PROTON_INCLUDE_DIRS proton/connection.hpp REQUIRED)
find_library(PROTON_CPP_LIB   NAMES qpid-proton-cpp REQUIRED)
find_library(PROTON_C_LIB     NAMES qpid-proton     REQUIRED)
set(PROTON_LIBRARIES ${PROTON_CPP_LIB} ${PROTON_C_LIB})

include(FetchContent)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(json)

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
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${PROTON_INCLUDE_DIRS}
        ${FFTW3F_INCLUDE_DIRS}
        # nlohmann/json is header-only; use the path, not the cmake target,
        # so it doesn't appear in the install export set.
        ${json_SOURCE_DIR}/include
)
target_link_libraries(sdrsdk
    PUBLIC  ${FFTW3F_LIBRARIES}
    PRIVATE ${PROTON_LIBRARIES}
)

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

# ── Install ───────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS sdrsdk EXPORT SdrSdkTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT SdrSdkTargets
    FILE      SdrSdkTargets.cmake
    NAMESPACE SdrSdk::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SdrSdk
)

# Package config so consumers can: find_package(SdrSdk REQUIRED)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SdrSdkConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    cmake/SdrSdkConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/SdrSdkConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SdrSdk
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SdrSdkConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SdrSdkConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SdrSdk
)
