cmake_minimum_required(VERSION 3.16)
project(Realsense_Server)
set(CMAKE_CXX_STANDARD 17)

find_package(realsense2 REQUIRED)
message(STATUS "${PROJECT_SOURCE_DIR}")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.cpp include/*.h)
#add_library(cam_server ${sources})
# Enable C++11
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
add_executable(${PROJECT_NAME} main.cpp ${sources})
target_include_directories(${PROJECT_NAME} PUBLIC include PRIVATE src)
target_link_libraries(${PROJECT_NAME} ${realsense2_LIBRARY} )