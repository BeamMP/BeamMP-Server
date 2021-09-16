cmake_minimum_required(VERSION 3.0)
project(Server)

if (WIN32)
    message(STATUS "MSVC -> forcing use of statically-linked runtime.")
    STRING(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    STRING(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    #-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
    set(VcpkgRoot ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET})
    include_directories(${VcpkgRoot}/include)
    link_directories(${VcpkgRoot}/lib)
elseif (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -static-libstdc++")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -s -fno-builtin")
    if (SANITIZE)
        message(STATUS "sanitize is ON")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined,thread")
    endif (SANITIZE)
endif ()

# this has to happen before -DDEBUG since it wont compile properly with -DDEBUG
add_subdirectory(deps)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

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
        include/TLuaPlugin.h src/TLuaPlugin.cpp
        include/TResourceManager.h src/TResourceManager.cpp
        include/THeartbeatThread.h src/THeartbeatThread.cpp
        include/Http.h src/Http.cpp
        include/TPPSMonitor.h src/TPPSMonitor.cpp
        include/TNetwork.h src/TNetwork.cpp
        include/LuaAPI.h src/LuaAPI.cpp)

include_directories("${PROJECT_SOURCE_DIR}/deps/asio/asio/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/asio/asio/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/rapidjson/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/websocketpp")
include_directories("${PROJECT_SOURCE_DIR}/deps/commandline")
include_directories("${PROJECT_SOURCE_DIR}/deps/sol2/include")
include_directories("${PROJECT_SOURCE_DIR}/deps")

include_directories(BeamMP-Server PUBLIC ${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Lua REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(BeamMP-Server sol2::sol2 ${LUA_LIBRARIES})

if (UNIX)
    target_link_libraries(BeamMP-Server z pthread stdc++fs crypto ${OPENSSL_LIBRARIES} commandline sioclient_tls)
elseif (WIN32)
    include(FindLua)
    find_package(ZLIB REQUIRED)
    find_package(RapidJSON CONFIG REQUIRED)
    target_include_directories(BeamMP-Server PRIVATE ${RAPIDJSON_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
    target_link_libraries(BeamMP-Server PRIVATE ws2_32 ZLIB::ZLIB ${OPENSSL_LIBRARIES} commandline sioclient_tls)
endif ()
