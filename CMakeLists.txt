
cmake_minimum_required (VERSION 2.8)

project (chat)

find_package (Boost REQUIRED COMPONENTS system)

list (APPEND CMAKE_CXX_FLAGS "-std=c++1y")

include_directories (.)

# client exe
add_executable (chat-client
    chat_client.cpp
    chat_structures.cpp
)

target_link_libraries (chat-client ${Boost_LIBRARIES} pthread)

# server exe
add_executable (chat-server
    chat_server.cpp
    server_session.cpp
    chat_structures.cpp
)

target_link_libraries (chat-server ${Boost_LIBRARIES} pthread)

