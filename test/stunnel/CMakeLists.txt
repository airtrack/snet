include_directories(/usr/local/include)
find_library(CRYPTO crypto /usr/local/lib)

add_executable(sclient
    Socks5.cpp
    Tunnel.cpp
    Cipher.cpp
    Client.cpp)

target_link_libraries(sclient snet ${CRYPTO})

add_executable(sserver
    Relay.cpp
    Tunnel.cpp
    Cipher.cpp
    Server.cpp)

target_link_libraries(sserver snet ${CRYPTO})
