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
