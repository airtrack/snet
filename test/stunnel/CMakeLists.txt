add_executable(sclient
    Socks5.cpp
    Tunnel.cpp
    Cipher.cpp
    Client.cpp)

include_directories(/usr/local/include)
find_library(CRYPTO crypto /usr/local/lib)

target_link_libraries(sclient snet ${CRYPTO})

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_executable(sserver
        Relay.cpp
        Tunnel.cpp
        Cipher.cpp
        Server.cpp)

    target_link_libraries(sserver snet ${CRYPTO})
endif()
