cmake_minimum_required(VERSION 3.15)
project(msomeip_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Set up mshm library
set(MSHM_LIBRARY /mnt/data/workspace/moss/mshm/build/src/libmshm.a)
set(MSHM_INCLUDE_DIR /mnt/data/workspace/moss/mshm/include)
add_library(mshm STATIC IMPORTED)
set_target_properties(mshm PROPERTIES IMPORTED_LOCATION ${MSHM_LIBRARY})
set_target_properties(mshm PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MSHM_INCLUDE_DIR}")

# Set up msomeip library
set(MSOMEIP_LIBRARY /mnt/data/workspace/moss/msomeip/build/libmsomeip.a)
set(MSOMEIP_INCLUDE_DIR /mnt/data/workspace/moss/msomeip/include)
add_library(msomeip STATIC IMPORTED)
set_target_properties(msomeip PROPERTIES IMPORTED_LOCATION ${MSOMEIP_LIBRARY})
set_target_properties(msomeip PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MSOMEIP_INCLUDE_DIR}")

# Examples CMakeLists.txt

# Example 1: Simple method call example
add_executable(example_method
    example_method.cpp
)

target_link_libraries(example_method
    msomeip
    Threads::Threads
)

# Example 2: Event subscription example
add_executable(example_event
    example_event.cpp
)

target_link_libraries(example_event
    msomeip
    Threads::Threads
)

# Example 3: Field example
add_executable(example_field
    example_field.cpp
)

target_link_libraries(example_field
    msomeip
    Threads::Threads
)

# Example 4: Service Discovery example
add_executable(example_discovery
    example_discovery.cpp
)

target_link_libraries(example_discovery
    msomeip
    Threads::Threads
)
