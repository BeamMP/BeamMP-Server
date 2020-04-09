cmake_minimum_required(VERSION 3.15)
project(BeamMP-Server)

set(CMAKE_CXX_STANDARD 14)

add_executable(BeamMP-Server src/main.cpp src/logger.cpp src/config.cpp src/Network/Server.cpp src/Network/enet.h src/Network/DataParser.cpp src/heartbeat.cpp src/heartbeat.h src/Network/ClientHandler.cpp src/Network/functions.cpp)
target_link_libraries(BeamMP-Server winmm ws2_32)