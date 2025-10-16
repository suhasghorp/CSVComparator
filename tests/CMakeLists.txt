find_package(GTest REQUIRED)

add_executable(csv_comparator_test
    csv_comparator_test.cpp
    ../src/row.cpp
    ../src/csv_parser.cpp
    ../src/csv_comparator.cpp
)

target_include_directories(csv_comparator_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/external/wyhash
)

target_link_libraries(csv_comparator_test PRIVATE
    GTest::gtest
    GTest::gtest_main
)

# Add Tracy if enabled
if(ENABLE_TRACY)
    target_link_libraries(csv_comparator_test PRIVATE TracyClient)
    target_compile_definitions(csv_comparator_test PRIVATE TRACY_ENABLE TRACY_ON_DEMAND)
endif()

# Platform-specific settings
if(MSVC)
    target_compile_options(csv_comparator_test PRIVATE /W4)
    target_compile_definitions(csv_comparator_test PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(csv_comparator_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add test to CTest
add_test(NAME CSVComparatorTest COMMAND csv_comparator_test)

set_tests_properties(CSVComparatorTest PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)