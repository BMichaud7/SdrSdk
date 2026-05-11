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
    add_subdirectory(../SdrTaskApi ${CMAKE_BINARY_DIR}/SdrTaskApi)
else()
    include(FetchContent)
    FetchContent_Declare(SdrTaskApi
        GIT_REPOSITORY https://github.com/BMichaud7/SdrTaskApi.git
        GIT_TAG        main/1.0
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SdrTaskApi)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3F REQUIRED fftw3f)

find_path(PROTON_INCLUDE_DIRS proton/connection.hpp REQUIRED)
find_library(PROTON_CPP_LIB   NAMES qpid-proton-cpp REQUIRED)
find_library(PROTON_C_LIB     NAMES qpid-proton     REQUIRED)
set(PROTON_LIBRARIES ${PROTON_CPP_LIB} ${PROTON_C_LIB})

# ── Library ───────────────────────────────────────────────────────────────────
add_library(sdrsdk
    src/SdrClient.cpp
    src/IqStream.cpp
    src/Spectrum.cpp
)
add_library(SdrSdk::sdrsdk ALIAS sdrsdk)

# Propagate SdrTaskApi include path without adding sdr_task_api to the export
# set (it is an PRIVATE implementation dep; consumers only need the headers).
get_target_property(_taskapi_inc sdr_task_api INTERFACE_INCLUDE_DIRECTORIES)

target_include_directories(sdrsdk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${_taskapi_inc}>   # exposes <sdr/Types.hpp>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${PROTON_INCLUDE_DIRS}
        ${FFTW3F_INCLUDE_DIRS}
)
target_link_libraries(sdrsdk
    PUBLIC  ${FFTW3F_LIBRARIES} ${PROTON_LIBRARIES}
    PRIVATE sdr_task_api
)
# Install SdrTaskApi headers as part of SdrSdk so consumers get <sdr/Types.hpp>
install(DIRECTORY ${_taskapi_inc}/sdr DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

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
install(TARGETS sdrsdk EXPORT SdrSdkTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
# sdr_task_api is a PRIVATE build dep; its headers are installed separately
# so the export set doesn't need to reference it as a cmake target.
set_target_properties(sdrsdk PROPERTIES
    INTERFACE_LINK_LIBRARIES "${FFTW3F_LIBRARIES};${PROTON_LIBRARIES}"
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT SdrSdkTargets
    FILE      SdrSdkTargets.cmake
    NAMESPACE SdrSdk::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SdrSdk
)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SdrSdkConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion)
configure_package_config_file(
    cmake/SdrSdkConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/SdrSdkConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SdrSdk)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SdrSdkConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SdrSdkConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SdrSdk)
