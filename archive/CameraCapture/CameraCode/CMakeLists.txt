cmake_minimum_required(VERSION 3.16)
project(CameraCapture)

set(CMAKE_CXX_STANDARD 17)

set(VimbaX_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Lib/VimbaX_2025-3") # ../ means go up 1 directory
include_directories(${VimbaX_ROOT_DIR}/api/include) # Header files
link_directories(${VimbaX_ROOT_DIR}/api/lib) # .so Files

add_executable(CameraCapture main.cpp)

target_link_libraries(CameraCapture
    VmbCPP
)