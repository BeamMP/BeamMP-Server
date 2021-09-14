cmake_minimum_required(VERSION 3.0)

message(STATUS "You can find build instructions and a list of dependencies in the README at \
https://github.com/BeamMP/BeamMP-Server")

project(BeamMP-Server
    DESCRIPTION "Server for BeamMP - The Multiplayer Mod for BeamNG.drive"
    HOMEPAGE_URL https://beammp.com
    LANGUAGES CXX C)

if (WIN32)
    # this has to happen before sentry, so that crashpad on windows links with these settings.
    message(STATUS "MSVC -> forcing use of statically-linked runtime.")
    STRING(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    STRING(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
endif()

include_directories("include/sentry-native/include")
set(SENTRY_BUILD_SHARED_LIBS OFF)
if (MSVC)
    set(SENTRY_BUILD_RUNTIMESTATIC ON)
endif()
set(SENTRY_BACKEND breakpad)
add_subdirectory("include/sentry-native")

message(STATUS "Setting compiler flags")
if (WIN32)
    #-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
    set(VcpkgRoot ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET})
    include_directories(${VcpkgRoot}/include)
    link_directories(${VcpkgRoot}/lib)
elseif (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -static-libstdc++")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -gz -fno-builtin")
    if (SANITIZE)
        message(STATUS "sanitize is ON")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLanAGS} -fsanitize=undefined,thread")
    endif (SANITIZE)
endif ()

message(STATUS "Checking for Sentry URL")
# this is set by the build system.
# IMPORTANT: if you're building from source, just leave this empty
if (NOT DEFINED BEAMMP_SECRET_SENTRY_URL)
    message(WARNING "No sentry URL configured. Sentry logging is disabled for this build. \
    This is not an error, and if you're building the BeamMP-Server yourself, this is expected and can be ignored.")
    set(BEAMMP_SECRET_SENTRY_URL "")
else()
    string(LENGTH ${BEAMMP_SECRET_SENTRY_URL} URL_LEN)
    message(STATUS "Sentry URL is length ${URL_LEN}")
endif()

message(STATUS "Adding local source dependencies")
# this has to happen before -DDEBUG since it wont compile properly with -DDEBUG
include_directories("asio/asio/include")
include_directories("rapidjson/include")
include_directories("websocketpp")
add_subdirectory("socket.io-client-cpp")
add_subdirectory("include/commandline")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

message(STATUS "Looking for Boost")
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
        include/TSentry.h src/TSentry.cpp
        include/TPPSMonitor.h src/TPPSMonitor.cpp
        include/TNetwork.h src/TNetwork.cpp
        include/SignalHandling.h src/SignalHandling.cpp)

target_compile_definitions(BeamMP-Server PRIVATE SECRET_SENTRY_URL="${BEAMMP_SECRET_SENTRY_URL}")

target_include_directories(BeamMP-Server PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/include" 
    "${CMAKE_CURRENT_SOURCE_DIR}/commandline")

message(STATUS "Looking for Lua")
find_package(Lua REQUIRED)
target_include_directories(BeamMP-Server PUBLIC 
    ${Boost_INCLUDE_DIRS} 
    ${LUA_INCLUDE_DIR} 
    ${CURL_INCLUDE_DIRS}
    "socket.io-client-cpp/src" 
    "include/tomlplusplus" 
    "include/sentry-native/include"
    "include/curl/include")

message(STATUS "Looking for SSL")
find_package(OpenSSL REQUIRED)

message(STATUS "CURL IS ${CURL_LIBRARIES}")

if (UNIX)
    target_link_libraries(BeamMP-Server 
        z 
        pthread 
        stdc++fs 
        ${LUA_LIBRARIES} 
        crypto
        ${OPENSSL_LIBRARIES} 
        commandline 
        sioclient_tls 
        sentry)
elseif (WIN32)
    include(FindLua)
    message(STATUS "Looking for libz")
    find_package(ZLIB REQUIRED)
    message(STATUS "Looking for RapidJSON")
    find_package(RapidJSON CONFIG REQUIRED)
    target_include_directories(BeamMP-Server PRIVATE ${RAPIDJSON_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
    target_link_libraries(BeamMP-Server PRIVATE 
        ws2_32 
        ZLIB::ZLIB 
        ${LUA_LIBRARIES} 
        ${OPENSSL_LIBRARIES} 
        commandline
        sioclient_tls
        sentry)
endif ()
