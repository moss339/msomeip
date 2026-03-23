# Tests CMakeLists.txt

# Enable testing
enable_testing()

# Find Google Test (using FetchContent for standalone build)
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(msomeip_tests
    test_message.cpp
    test_payload.cpp
    test_service_discovery.cpp
)

target_link_libraries(msomeip_tests
    msomeip
    gtest
    gtest_main
    Threads::Threads
)

# Add tests
add_test(NAME msomeip_unit_tests COMMAND msomeip_tests)
