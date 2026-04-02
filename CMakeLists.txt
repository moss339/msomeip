cmake_minimum_required(VERSION 3.14)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

project(msomeip VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" OFF)

find_package(Threads REQUIRED)

# Create mshm imported target using relative path
set(MSHM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../mshm" CACHE PATH "mshm root directory")
if(EXISTS "${MSHM_ROOT}/build/src/libmshm.so")
    set(MSHM_LIBRARY "${MSHM_ROOT}/build/src/libmshm.so")
else()
    set(MSHM_LIBRARY "${MSHM_ROOT}/src/libmshm.so")
endif()
set(MSHM_INCLUDE_DIR "${MSHM_ROOT}/include")

add_library(mshm::mshm SHARED IMPORTED)
set_target_properties(mshm::mshm PROPERTIES
    IMPORTED_LOCATION "${MSHM_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${MSHM_INCLUDE_DIR}"
)

# Source files
set(MSOMEIP_SOURCES
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

if(BUILD_SHARED_LIBS)
    add_library(msomeip SHARED ${MSOMEIP_SOURCES})
    set_target_properties(msomeip PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        POSITION_INDEPENDENT_CODE ON
    )
else()
    add_library(msomeip STATIC ${MSOMEIP_SOURCES})
    set_target_properties(msomeip PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
    )
endif()

target_include_directories(msomeip PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${MSHM_INCLUDE_DIR}
)

target_link_libraries(msomeip PUBLIC
    Threads::Threads
    mshm::mshm
)

install(TARGETS msomeip
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
