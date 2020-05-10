cmake_minimum_required(VERSION 3.15)
project(BeamMP-Server)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
include_directories(${PROJECT_SOURCE_DIR}/curl)

set(CMAKE_CXX_STANDARD 14)

file(GLOB source_files
        "src/*.h" "src/*.hpp" "src/*.cpp"
        "src/Network 2.0/*.hpp" "src/Network 2.0/*.cpp"
        "src/curl/*.h")

add_executable(BeamMP-Server ${source_files})
target_link_libraries(BeamMP-Server libcurl_a ws2_32)