cmake_minimum_required(VERSION 3.14)
project(msomeip VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# Find packages
find_package(Threads REQUIRED)

# Find mshm
set(MSHM_ROOT "/mnt/data/workspace/moss/mshm" CACHE PATH "mshm root directory")
set(MSHM_LIBRARY "/mnt/data/workspace/moss/mshm/build/src/libmshm.a" CACHE FILEPATH "mshm library")
set(MSHM_INCLUDE_DIR "/mnt/data/workspace/moss/mshm/include" CACHE PATH "mshm include directory")

# Create imported target for mshm
add_library(mshm STATIC IMPORTED)
set_target_properties(mshm PROPERTIES
    IMPORTED_LOCATION "${MSHM_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${MSHM_INCLUDE_DIR}"
)

# Main library
add_library(msomeip STATIC
    src/message/message.cpp
    src/message/payload.cpp
    src/sd/service_discovery.cpp
    src/sd/service_entry.cpp
    src/transport/udp_transport.cpp
    src/transport/tcp_transport.cpp
    src/runtime.cpp
    src/application.cpp
    src/service_base.cpp
    src/shm_agent.cpp
)

target_include_directories(msomeip
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${MSHM_INCLUDE_DIR}
)

target_link_libraries(msomeip
    PUBLIC
        Threads::Threads
        mshm
)

target_compile_options(msomeip PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
