cmake_minimum_required(VERSION 3.20)
project(CSVComparator VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Tracy profiler option
option(ENABLE_TRACY "Enable Tracy profiling" OFF)

if(ENABLE_TRACY)
    message(STATUS "Tracy profiling enabled")
    # Tracy options
    option(TRACY_ENABLE "" ON)
    option(TRACY_ON_DEMAND "" ON)
    option(TRACY_NO_FRAME_IMAGE "" ON)
    
    # Add Tracy
    add_subdirectory(external/tracy)
endif()


# Add source directory
add_subdirectory(src)

# Enable testing
enable_testing()
add_subdirectory(tests)

# Installation rules
install(TARGETS file_compare
    RUNTIME DESTINATION bin
)
