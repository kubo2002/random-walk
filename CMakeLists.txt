cmake_minimum_required(VERSION 3.5)
project(random_walk C)

set(CMAKE_C_STANDARD 11)

# Ciele pre kompiláciu
add_executable(server
        server/src/server.c
        server/src/server_net.c
        common/protocol_io.c
        server/src/simulation.c
        server/src/world.c
)

target_include_directories(server PRIVATE
        server/headers
        common
)

add_executable(client
        client/src/client.c
        client/src/client_net.c
        server/src/server_net.c
        common/protocol_io.c
        client/src/render.c
        client/src/menu.c
)

target_include_directories(client PRIVATE
        client/headers
        common
)

# clean a all su uz preddefinovane by default v CMake, dali by sa zadefinovat len ako aliases

target_link_libraries(server pthread)
target_link_libraries(client pthread)
