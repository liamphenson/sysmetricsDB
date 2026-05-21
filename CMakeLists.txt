cmake_minimum_required(VERSION 3.20)

# Project metadata
project(SysMetricsDB VERSION 1.0 LANGUAGES CXX)

# Force C++20 Standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include the headers folder so the compiler can find them
include_directories(include)

# Define the executable and the files needed to build it
add_executable(SysMetricsDB 
    src/main.cpp
    src/MetricSeries.cpp
    src/Database.cpp
    src/WindowsPoller.cpp
 "src/MetricSeries.cpp" "src/Database.cpp" "src/WindowsPoller.cpp" )

# Find the httplib package
find_package(httplib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# Look for the CUDA toolkit environment variable
find_path(NVML_INCLUDE_DIR nvml.h PATHS "$ENV{CUDA_PATH}/include")
find_library(NVML_LIBRARY nvml PATHS "$ENV{CUDA_PATH}/lib/x64")

# Link the httplib library to our executable
target_link_libraries(SysMetricsDB PRIVATE 
    httplib::httplib
    glfw
    imgui::imgui
    implot::implot
    OpenGL::GL
)

if(NVML_INCLUDE_DIR AND NVML_LIBRARY)
    target_include_directories(SysMetricsDB PRIVATE ${NVML_INCLUDE_DIR})
    target_link_libraries(SysMetricsDB PRIVATE ${NVML_LIBRARY})
else()
    message(FATAL_ERROR "NVML SDK not found. Please ensure the CUDA toolkit is installed and the CUDA_PATH environment variable is set.")
endif()

# Copy assets directory to the output directory
add_custom_command(TARGET SysMetricsDB POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:SysMetricsDB>/assets
)

# Set warnings to a high level
if(MSVC)
    target_compile_options(SysMetricsDB PRIVATE /W4 /WX)
else()
    target_compile_options(SysMetricsDB PRIVATE -Wall -Wextra -Werror)
endif()

if(WIN32)
    target_link_libraries(SysMetricsDB PRIVATE iphlpapi ws2_32)
endif()