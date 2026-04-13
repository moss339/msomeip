add_executable(service_example service_example.cpp)
target_link_libraries(service_example PRIVATE msomeip Threads::Threads)

add_executable(event_example event_example.cpp)
target_link_libraries(event_example PRIVATE msomeip Threads::Threads)
