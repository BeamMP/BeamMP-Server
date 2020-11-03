cmake_minimum_required(VERSION 3.10)
project(Server)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -s")
elseif (WIN32)
    # This might cause issues with old windows headers, but it's worth the trouble to keep the code
    #   completely cross platform. For fixes to common issues arising from /permissive- visit:
    #   https://docs.microsoft.com/en-us/cpp/build/reference/permissive-standards-conformance
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /permissive-")
endif ()

find_package(Boost 1.71.0 REQUIRED COMPONENTS system thread)
file(GLOB source_files "src/*.cpp" "src/*/*.cpp" "include/*.h" "include/*/*.h" "include/*.hpp" "include/*/*.hpp")
add_executable(BeamMP-Server ${source_files})

target_include_directories(BeamMP-Server PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> ${Boost_INCLUDE_DIRS})

if (UNIX)
    target_link_libraries(BeamMP-Server curl lua5.3 krb5 z pthread stdc++fs ${Boost_LINK_DIRS})
elseif (WIN32)
    target_link_libraries(BeamMP-Server libcurl_a urlmon ws2_32 lua53 zlibstatic ${Boost_LINK_DIRS})
endif ()
