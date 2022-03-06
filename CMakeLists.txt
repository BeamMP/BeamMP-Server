cmake_minimum_required(VERSION 3.0)

message(STATUS "You can find build instructions and a list of dependencies in the README at \
https://github.com/BeamMP/BeamMP-Server")

project(BeamMP-Server
    DESCRIPTION "Server for BeamMP - The Multiplayer Mod for BeamNG.drive"
    HOMEPAGE_URL https://beammp.com
    LANGUAGES CXX C)

set(HTTPLIB_REQUIRE_OPENSSL ON)

include_directories("${PROJECT_SOURCE_DIR}/deps/asio/asio/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/rapidjson/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/websocketpp")
include_directories("${PROJECT_SOURCE_DIR}/deps/commandline")
include_directories("${PROJECT_SOURCE_DIR}/deps/sol2/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/cpp-httplib")
include_directories("${PROJECT_SOURCE_DIR}/deps/json/single_include") # nlohmann
include_directories("${PROJECT_SOURCE_DIR}/deps")

add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT)

if(APPLE)
    set(LUA_INCLUDE_DIR /usr/local/Cellar/lua@5.3/5.3.6/include/lua5.3)
    set(LUA_LIBRARIES lua)
    include_directories(/usr/local/opt/openssl@1.1/include)
    link_directories(/usr/local/Cellar/lua@5.3/5.3.6/lib)
    link_directories(/usr/local/opt/openssl@1.1/lib)
endif()

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
add_subdirectory("deps/sentry-native")

if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /bigobj")
endif ()

message(STATUS "Setting compiler flags")
if (WIN32)

    #-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
    set(VcpkgRoot ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET})
    include_directories(${VcpkgRoot}/include)
    link_directories(${VcpkgRoot}/lib)
elseif (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -static-libstdc++")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -fno-builtin")
    if (SANITIZE)
        message(STATUS "sanitize is ON")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined,thread")
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
add_subdirectory(deps)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

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
        include/TSentry.h src/TSentry.cpp
        include/TPPSMonitor.h src/TPPSMonitor.cpp
        include/TNetwork.h src/TNetwork.cpp
        include/LuaAPI.h src/LuaAPI.cpp
        include/TScopedTimer.h src/TScopedTimer.cpp
        include/SignalHandling.h src/SignalHandling.cpp
        include/ArgsParser.h src/ArgsParser.cpp
        include/Environment.h)

target_compile_definitions(BeamMP-Server PRIVATE SECRET_SENTRY_URL="${BEAMMP_SECRET_SENTRY_URL}")
include_directories(BeamMP-Server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_include_directories(BeamMP-Server PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/include" 
    "${CMAKE_CURRENT_SOURCE_DIR}/commandline")

if (APPLE)
    message(STATUS "NOT looking for Lua on APPLE")
else()
    message(STATUS "Looking for Lua")
    find_package(Lua REQUIRED VERSION 5.3)
endif()

target_include_directories(BeamMP-Server PUBLIC 
    ${LUA_INCLUDE_DIR} 
    ${CURL_INCLUDE_DIRS}
    "include/tomlplusplus" 
    "include/sentry-native/include"
    "include/curl/include")

message(STATUS "Looking for SSL")

if (APPLE)
    set(OPENSSL_LIBRARIES ssl crypto)
else()
    find_package(OpenSSL REQUIRED)
endif()

target_link_libraries(BeamMP-Server sol2::sol2 ${LUA_LIBRARIES} nlohmann_json_schema_validator)
message(STATUS "CURL IS ${CURL_LIBRARIES}")

if (UNIX)
    target_link_libraries(BeamMP-Server
        z 
        pthread
        ${LUA_LIBRARIES} 
        crypto
        ${OPENSSL_LIBRARIES} 
        commandline 
        sentry
        ssl)
elseif (WIN32)
    include(FindLua)
    message(STATUS "Looking for libz")
    find_package(ZLIB REQUIRED)
    message(STATUS "Looking for RapidJSON")
    find_package(RapidJSON CONFIG REQUIRED)
    target_include_directories(BeamMP-Server PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
    target_link_libraries(BeamMP-Server
        ws2_32 
        ZLIB::ZLIB 
        ${LUA_LIBRARIES} 
        ${OPENSSL_LIBRARIES} 
        commandline
        sentry)
endif ()
