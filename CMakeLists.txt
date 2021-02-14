cmake_minimum_required(VERSION 3.13)
project(Server)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

add_subdirectory("include/commandline")

if (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -static-libstdc++")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -s -fno-builtin")
elseif (WIN32)
    message(STATUS "MSVC -> forcing use of statically-linked runtime.")
    STRING(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    STRING(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    #-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
endif ()

find_package(Boost REQUIRED COMPONENTS system thread)

add_executable(BeamMP-Server 
    src/main.cpp
    src/TConsole.cpp
    src/Compat.cpp
)
    
target_include_directories(BeamMP-Server PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/commandline")

find_package(Lua REQUIRED)
target_include_directories(BeamMP-Server PUBLIC ${Boost_INCLUDE_DIRS} ${LUA_INCLUDE_DIR})

find_package(OpenSSL REQUIRED)

if (UNIX)
    target_link_libraries(BeamMP-Server z pthread stdc++fs ${Boost_LINK_DIRS} ${LUA_LIBRARIES} dl crypto ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} commandline)
elseif (WIN32)
    include(FindLua)
    find_package(ZLIB REQUIRED)
    find_package(RapidJSON CONFIG REQUIRED)
    target_include_directories(BeamMP-Server PRIVATE ${RAPIDJSON_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
    target_link_libraries(BeamMP-Server PRIVATE ws2_32 ZLIB::ZLIB ${LUA_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} commandline)
endif ()
