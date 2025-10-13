add_executable(csv_compare
    main.cpp
    row.cpp
    csv_parser.cpp
    threaded_comparator.cpp
)

target_include_directories(csv_compare PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(csv_compare PRIVATE
    Boost::boost
    Boost::system
    Threads::Threads
)

# Add Tracy to main executable (optional, for profiling main app)
if(ENABLE_TRACY)
    target_link_libraries(csv_compare PRIVATE TracyClient)
    target_compile_definitions(csv_compare PRIVATE TRACY_ENABLE TRACY_ON_DEMAND)
endif()

# Platform-specific settings
if(MSVC)
    target_compile_options(csv_compare PRIVATE /W4 /WX)
else()
    target_compile_options(csv_compare PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()