cmake_minimum_required(VERSION 3.21)
project(chat)

set(CMAKE_CXX_STANDARD 11)

add_executable(server server.cpp)
add_executable(client client.cpp)

target_link_libraries(server PRIVATE wsock32 ws2_32)
target_link_libraries(client PRIVATE wsock32 ws2_32)
