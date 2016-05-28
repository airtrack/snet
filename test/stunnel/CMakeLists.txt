add_executable(sclient
    Tunnel.cpp
    Client.cpp)

target_link_libraries(sclient snet)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_executable(sserver
        Relay.cpp
        Tunnel.cpp
        Server.cpp)

    target_link_libraries(sserver snet)
endif()
