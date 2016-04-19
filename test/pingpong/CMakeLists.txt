add_executable(client Client.cpp)
add_executable(server Server.cpp)

target_link_libraries(client snet)
target_link_libraries(server snet)
