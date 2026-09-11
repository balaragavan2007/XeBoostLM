cmake_minimum_required(VERSION 3.10)
project(XeBoostLM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tell CMake where our header menu is
include_directories(include)

# Automatically find Intel's libraries
find_package(OpenVINO REQUIRED)
find_package(OpenVINOGenAI REQUIRED)

# Collect the kitchen files
set(SOURCES 
    src/main.cpp 
    src/memory_profiler.cpp 
    src/downloader.cpp 
    src/inference_engine.cpp
)

add_executable(xeboost ${SOURCES})

# Link the OpenVINO engine across all OS platforms
target_link_libraries(xeboost PRIVATE openvino::runtime openvino::genai)

# Only link libcurl if we are compiling on Linux/WSL
if(UNIX)
    target_link_libraries(xeboost PRIVATE curl)
endif()
