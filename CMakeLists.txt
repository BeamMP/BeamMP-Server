cmake_minimum_required(VERSION 3.10)
project(Server)
set(CMAKE_CXX_STANDARD 17)

if (WIN32)
    message(STATUS "MSVC -> forcing use of statically-linked runtime.")
    STRING(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    STRING(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    #-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
endif ()

# this has to happen before -DDEBUG since it wont compile properly with -DDEBUG
include_directories("asio/asio/include")
include_directories("rapidjson/include")
include_directories("websocketpp")
add_subdirectory("socket.io-client-cpp")
add_subdirectory("include/commandline")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -static-libstdc++")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -s -fno-builtin")
    if (SANITIZE)
        message(STATUS "sanitize is ON")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined,thread")
    endif (SANITIZE)
endif()

find_package(Boost REQUIRED COMPONENTS system thread)

add_executable(BeamMP-Server
        src/main.cpp
        include/TConsole.h src/TConsole.cpp
        include/TServer.h src/TServer.cpp
        include/Compat.h src/Compat.cpp
        include/Common.h src/Common.cpp
        include/Client.h src/Client.cpp
        include/VehicleData.h src/VehicleData.cpp
        include/TConfig.h src/TConfig.cpp
        include/TLuaEngine.h src/TLuaEngine.cpp
        include/TLuaFile.h src/TLuaFile.cpp
        include/TResourceManager.h src/TResourceManager.cpp
        include/THeartbeatThread.h src/THeartbeatThread.cpp
        include/Http.h src/Http.cpp
        include/SocketIO.h src/SocketIO.cpp
        include/TPPSMonitor.h src/TPPSMonitor.cpp
        include/TUDPServer.h src/TUDPServer.cpp
        include/TTCPServer.h src/TTCPServer.cpp)

target_include_directories(BeamMP-Server PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/commandline")

find_package(Lua REQUIRED)
target_include_directories(BeamMP-Server PUBLIC ${Boost_INCLUDE_DIRS} ${LUA_INCLUDE_DIR} "socket.io-client-cpp/src")

find_package(OpenSSL REQUIRED)

if (UNIX)
    target_link_libraries(BeamMP-Server z pthread stdc++fs ${Boost_LINK_DIRS} ${LUA_LIBRARIES} dl crypto ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} commandline sioclient_tls)
elseif (WIN32)
    include(FindLua)
    find_package(ZLIB REQUIRED)
    find_package(RapidJSON CONFIG REQUIRED)
    target_include_directories(BeamMP-Server PRIVATE ${RAPIDJSON_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
    target_link_libraries(BeamMP-Server PRIVATE ws2_32 ZLIB::ZLIB ${LUA_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} commandline sioclient_tls)
endif ()
