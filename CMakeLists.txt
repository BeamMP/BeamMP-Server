cmake_minimum_required(VERSION 3.10)
project(Server)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -static-libstdc++")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -s")
elseif (WIN32)
    # This might cause issues with old windows headers, but it's worth the trouble to keep the code
    #   completely cross platform. For fixes to common issues arising from /permissive- visit:
    #   https://docs.microsoft.com/en-us/cpp/build/reference/permissive-standards-conformance
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /permissive-")
    message(STATUS "MSVC -> forcing use of statically-linked runtime.")
    STRING(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    STRING(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    #-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
endif ()

find_package(Boost 1.70.0 REQUIRED COMPONENTS system thread)

file(GLOB source_files "src/*.cpp" "src/*/*.cpp" "include/*.h" "include/*/*.h" "include/*.hpp" "include/*/*.hpp")
add_executable(BeamMP-Server ${source_files})
    
target_include_directories(BeamMP-Server SYSTEM PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
find_package(Lua 5.3 REQUIRED)
target_include_directories(BeamMP-Server SYSTEM PUBLIC ${LUA_INCLUDE_DIR} ${Boost_INCLUDE_DIRS})

if (UNIX)
    target_link_libraries(BeamMP-Server z pthread stdc++fs ${Boost_LINK_DIRS} lua curl dl)
elseif (WIN32)
    include(FindLua)
    find_package(CURL CONFIG REQUIRED)
    find_package(ZLIB REQUIRED)
    target_link_libraries(BeamMP-Server PRIVATE urlmon ws2_32 CURL::libcurl ZLIB::ZLIB ${Boost_LINK_DIRS} ${LUA_LIBRARIES})
endif ()
