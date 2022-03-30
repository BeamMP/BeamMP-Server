# 3.4 is required for imported targets.
cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

message(STATUS "You can find build instructions and a list of dependencies in the README at \
https://github.com/BeamMP/BeamMP-Server")

project(BeamMP-Server
    DESCRIPTION "Server for BeamMP - The Multiplayer Mod for BeamNG.drive"
    HOMEPAGE_URL https://beammp.com
    LANGUAGES CXX C)

find_package(Git REQUIRED)
# Update submodules as needed
option(GIT_SUBMODULE "Check submodules during build" ON)
if(GIT_SUBMODULE)
    message(STATUS "Submodule update")
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
    endif()
endif()


set(HTTPLIB_REQUIRE_OPENSSL ON)
set(SENTRY_BUILD_SHARED_LIBS OFF)

include_directories("${PROJECT_SOURCE_DIR}/deps/asio/asio/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/rapidjson/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/websocketpp")
include_directories("${PROJECT_SOURCE_DIR}/deps/commandline")
include_directories("${PROJECT_SOURCE_DIR}/deps/sol2/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/cpp-httplib")
include_directories("${PROJECT_SOURCE_DIR}/deps/json/single_include")
include_directories("${PROJECT_SOURCE_DIR}/deps")

add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT)

# ------------------------ APPLE ---------------------------------
if(APPLE)
    set(LUA_INCLUDE_DIR /usr/local/Cellar/lua@5.3/5.3.6/include/lua5.3)
    set(LUA_LIBRARIES lua)
    include_directories(/usr/local/opt/openssl@1.1/include)
    link_directories(/usr/local/Cellar/lua@5.3/5.3.6/lib)
    link_directories(/usr/local/opt/openssl@1.1/lib)
# ------------------------ WINDOWS ---------------------------------
elseif (WIN32)
    # this has to happen before sentry, so that crashpad on windows links with these settings.
    message(STATUS "MSVC -> forcing use of statically-linked runtime.")
    STRING(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    STRING(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    if (MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
        set(SENTRY_BUILD_RUNTIMESTATIC ON)
    endif ()
    set(VcpkgRoot ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET})
    include_directories(${VcpkgRoot}/include)
    link_directories(${VcpkgRoot}/lib)
# ------------------------ LINUX ---------------------------------
elseif (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -static-libstdc++ -static-libgcc")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -fno-builtin")
    if (SANITIZE)
        message(STATUS "sanitize is ON")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined,thread")
    endif (SANITIZE)
endif ()

include_directories("include/sentry-native/include")

# ------------------------ SENTRY ---------------------------------
message(STATUS "Checking for Sentry URL")
# this is set by the build system.
# IMPORTANT: if you're building from source, just leave this empty
if (NOT DEFINED BEAMMP_SECRET_SENTRY_URL)
    message(WARNING "No sentry URL configured. Sentry logging is disabled for this build. \
    This is not an error, and if you're building the BeamMP-Server yourself, this is expected and can be ignored.")
    set(BEAMMP_SECRET_SENTRY_URL "")
    set(SENTRY_BACKEND none)
else()
    set(SENTRY_BACKEND breakpad)
endif()
add_subdirectory("deps/sentry-native")

# ------------------------ C++ SETUP ---------------------------------
set(CMAKE_CXX_STANDARD 17)

# ------------------------ DEPENDENCIES ------------------------------
add_subdirectory(deps)

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

include(FindLua)
include(FindOpenSSL)
include(FindThreads)
include(FindZLIB)

target_include_directories(BeamMP-Server PUBLIC 
    ${LUA_INCLUDE_DIR} 
    ${CURL_INCLUDE_DIRS}
    "include/tomlplusplus" 
    "include/sentry-native/include"
    "include/curl/include")

target_link_libraries(BeamMP-Server 
    OpenSSL::SSL
    OpenSSL::Crypto
    sol2::sol2
    fmt::fmt
    Threads::Threads
    ZLIB::ZLIB
    ${LUA_LIBRARIES}
    commandline
    sentry)

if (WIN32)
    target_link_libraries(BeamMP-Server wsock32 ws2_32)
endif ()
