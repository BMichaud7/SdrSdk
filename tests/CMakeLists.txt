include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(googletest)

add_executable(sdrsdk_tests
    test_types.cpp
    test_spectrum.cpp
    test_iqstream.cpp
)
target_link_libraries(sdrsdk_tests PRIVATE sdrsdk GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(sdrsdk_tests)
