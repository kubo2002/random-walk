cmake_minimum_required(VERSION 3.5)
project(random_walk C)

set(CMAKE_C_STANDARD 11)

add_executable(server
        server/src/server.c
        server/src/server_net.c
        server/headers/server.h
        server/headers/simulation.h
        server/headers/world.h
        server/src/simulation.c
        server/src/world.c
        client/headers/client.h
        client/headers/client_net.h
        client/headers/menu.h
        client/headers/render.h
)

target_include_directories(server PRIVATE
        server/headers
        common
)

add_executable(client
        client/src/client.c
        client/src/client_net.c
        client/src/menu.c
        client/src/render.c
        server/headers/server.h
        server/headers/server_net.h
        server/headers/simulation.h
        server/headers/world.h
        server/src/simulation.c
        server/src/world.c
        client/headers/client.h
        client/headers/client_net.h
        client/headers/menu.h
        client/headers/render.h
        common/config.h
        common/protocol.h
)

target_include_directories(client PRIVATE
        client/headers
        common
)

target_link_libraries(server pthread)
target_link_libraries(client pthread)
