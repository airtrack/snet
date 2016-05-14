cmake_minimum_required(VERSION 3.1)

project(snet)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_CXX_FLAGS
        "-std=c++11 -Wall -Wextra -Wno-unused-parameter -O2"
        )
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_CXX_FLAGS
        "-std=c++11 -Wall -Wextra -Wno-unused-parameter -O2 -pthread"
        )
endif()

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")

add_library(snet
    Acceptor.cpp
    Connector.cpp
    Connection.cpp
    EventLoop.cpp
    SocketOps.cpp
    )

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_sources(snet
        PRIVATE KQueue.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_sources(snet
        PRIVATE Epoll.cpp)
endif()

add_subdirectory(test)
