cmake_minimum_required(VERSION 3.15)
project(BeamNG-MP-Server)

set(CMAKE_CXX_STANDARD 14)

add_executable(cmake-main src/main.cpp src/network.cpp src/logger.cpp)
ADD_LIBRARY(
	enet STATIC
	enet.lib
)
set_target_properties(enet PROPERTIES LINKER_LANGUAGE CXX)