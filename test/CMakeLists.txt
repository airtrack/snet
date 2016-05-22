include_directories(${PROJECT_SOURCE_DIR})

add_subdirectory(pingpong)
add_subdirectory(message_queue)
add_subdirectory(timer)


if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_subdirectory(addrinfo_resolve)
endif()
