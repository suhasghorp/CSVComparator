find_package(GTest REQUIRED)
find_package(xlnt CONFIG REQUIRED)

add_executable(file_comparator_test
    file_comparator_test.cpp
    ../src/row.cpp
    ../src/csv_parser.cpp
    ../src/file_type.cpp
    ../src/file_comparator.cpp
)

target_include_directories(file_comparator_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/external/wyhash
)

target_link_libraries(file_comparator_test PRIVATE
    GTest::gtest
    GTest::gtest_main
    xlnt::xlnt
)

# Add Tracy if enabled
if(ENABLE_TRACY)
    target_link_libraries(file_comparator_test PRIVATE TracyClient)
    target_compile_definitions(file_comparator_test PRIVATE TRACY_ENABLE TRACY_ON_DEMAND)
endif()
# Platform-specific settings
if(MSVC)
    target_compile_options(file_comparator_test PRIVATE /W4)
    target_compile_definitions(file_comparator_test PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(file_comparator_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add test to CTest
add_test(NAME FileComparatorTest COMMAND file_comparator_test)

set_tests_properties(FileComparatorTest PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)