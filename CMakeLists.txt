cmake_minimum_required(VERSION 2.8.4)
project(SecServer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

find_package(LibMicroHTTPD REQUIRED)


set(SOURCE_FILES
    src/main.cpp
    src/webserver/webserver.cpp src/webserver/webserver.h
)
add_executable(SecServer ${SOURCE_FILES})
target_link_libraries(SecServer ${MHD_LIBRARIES})