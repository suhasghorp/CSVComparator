find_package(xlnt CONFIG REQUIRED)

add_executable(file_compare
    main.cpp
    row.cpp
    csv_parser.cpp
    file_type.cpp
    file_comparator.cpp
)

target_include_directories(file_compare PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/external/wyhash
)

target_link_libraries(file_compare PRIVATE
    xlnt::xlnt
)

# Add Tracy to main executable (optional, for profiling main app)
if(ENABLE_TRACY)
    target_link_libraries(file_compare PRIVATE TracyClient)
    target_compile_definitions(file_compare PRIVATE TRACY_ENABLE TRACY_ON_DEMAND)
endif()

# Platform-specific settings
if(MSVC)
    target_compile_options(file_compare PRIVATE /W4 /WX)
else()
    target_compile_options(file_compare PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()