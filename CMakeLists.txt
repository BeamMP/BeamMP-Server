cmake_minimum_required(VERSION 3.13)
project(Server)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -static-libstdc++")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -s -fno-builtin")
elseif (WIN32)
    # This might cause issues with old windows headers, but it's worth the trouble to keep the code
    #   completely cross platform. For fixes to common issues arising from /permissive- visit:
    #   https://docs.microsoft.com/en-us/cpp/build/reference/permissive-standards-conformance

    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /permissive-")
    message(STATUS "MSVC -> forcing use of statically-linked runtime.")
    STRING(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    STRING(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    #-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
endif ()

#find_package(Boost 1.70.0 REQUIRED COMPONENTS system thread)

file(GLOB source_files "src/*.cpp" "include/*.h" "include/*/*.h" "include/*/*/*.h" "include/*.hpp" "include/*/*.hpp" "src/*/*.cpp")
add_executable(BeamMP-Server ${source_files})
    
target_include_directories(BeamMP-Server PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> cpp-httplib)

find_package(Lua REQUIRED)
target_include_directories(BeamMP-Server PUBLIC ${LUA_INCLUDE_DIR})

if (UNIX)
    target_link_libraries(BeamMP-Server z pthread stdc++fs ${Boost_LINK_DIRS} ${LUA_LIBRARIES} curl dl)
elseif (WIN32)
    include(FindLua)
    find_package(ZLIB REQUIRED)
    find_package(CURL CONFIG REQUIRED)
    find_package(RapidJSON CONFIG REQUIRED)
    target_include_directories(BeamMP-Server PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
    target_link_libraries(BeamMP-Server PRIVATE ws2_32 CURL::libcurl ZLIB::ZLIB ${LUA_LIBRARIES})
    #${Boost_LINK_DIRS}
endif ()
