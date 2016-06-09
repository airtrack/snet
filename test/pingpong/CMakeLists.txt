add_executable(pclient Client.cpp)
add_executable(pserver Server.cpp)

target_link_libraries(pclient snet)
target_link_libraries(pserver snet)
