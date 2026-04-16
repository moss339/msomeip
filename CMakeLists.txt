cmake_minimum_required(VERSION 3.14)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" OFF)

project(msomeip VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(mshm REQUIRED)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Source files
set(MSOMEIP_SOURCES
    src/message/message.cpp
    src/message/payload.cpp
    src/sd/service_discovery.cpp
    src/sd/service_entry.cpp
    src/transport/udp_transport.cpp
    src/transport/tcp_transport.cpp
    src/tp/tp_segmenter.cpp
    src/tp/tp_reassembler.cpp
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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(msomeip PUBLIC
    Threads::Threads
    mshm::mshm
)

# Export msomeip target
export(TARGETS msomeip NAMESPACE msomeip:: FILE cmake/msomeipTargets.cmake)

install(TARGETS msomeip
    EXPORT msomeipTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(EXPORT msomeipTargets
    FILE msomeipTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/msomeip
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/msomeipConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/msomeipConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/msomeip
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cmake/msomeipConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/msomeip
)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
