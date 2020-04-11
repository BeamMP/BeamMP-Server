cmake_minimum_required(VERSION 3.15)
project(BeamMP-Server)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
include_directories(${PROJECT_SOURCE_DIR}/curl)

set(CMAKE_CXX_STANDARD 14)

add_executable(BeamMP-Server src/main.cpp src/http.cpp src/logger.cpp src/config.cpp src/Network/Server.cpp
        src/Network/enet.h src/Network/DataParser.cpp src/heartbeat.cpp
        src/Network/ClientHandler.cpp src/Network/functions.cpp src/Settings.hpp
         src/Resources.cpp src/Network/TCPClientHandler.cpp)
target_link_libraries(BeamMP-Server winmm ws2_32 libcurl_a)