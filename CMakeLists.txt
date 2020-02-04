cmake_minimum_required(VERSION 3.15)
project(BeamNG-MP-Server)

set(CMAKE_CXX_STANDARD 14)

add_executable(BeamNG-MP-Server src/main.cpp src/logger.cpp src/config.cpp src/Network/Server.cpp src/Network/enet.h src/Network/DataParser.cpp src/heartbeat.cpp src/heartbeat.h)
target_link_libraries(BeamNG-MP-Server winmm ws2_32)