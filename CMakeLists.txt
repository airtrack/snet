cmake_minimum_required(VERSION 2.6)

project(snet)

set(CMAKE_CXX_FLAGS
    "-std=c++11 -Wall -Wextra -Wno-unused-parameter -O2"
    )

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")

add_library(snet
    Acceptor.cpp
    Connector.cpp
    Connection.cpp
    KQueue.cpp
    )
