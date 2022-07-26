# 3.4 is required for imported targets.
cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

message(STATUS "You can find build instructions and a list of dependencies in the README at \
https://github.com/BeamMP/BeamMP-Server")

project(BeamMP-Server
    DESCRIPTION "Server for BeamMP - The Multiplayer Mod for BeamNG.drive"
    HOMEPAGE_URL https://beammp.com
    LANGUAGES CXX C)

option(BEAMMP_RUN_GIT "Run git to make sure submodules are updated (leave this on unless you know what you're doing)" ON)

if (BEAMMP_RUN_GIT)
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
endif()


set(HTTPLIB_REQUIRE_OPENSSL ON)
set(SENTRY_BUILD_SHARED_LIBS OFF)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps/asio/asio/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps/rapidjson/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps/websocketpp")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps/commandline")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps/sol2/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps/cpp-httplib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps/json/single_include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps")

add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT=1)

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
# ------------------------ LINUX ---------------------------------
elseif (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -fno-builtin")
    option(SANITIZE "Turns on thread and UB sanitizers" OFF)
    if (SANITIZE)
        message(STATUS "sanitize is ON")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined,thread")
    endif (SANITIZE)
endif ()

include_directories("include/sentry-native/include")
set(BUILD_SHARED_LIBS OFF)
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
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
endif ()

# ------------------------ DEPENDENCIES ------------------------------
message(STATUS "Adding local source dependencies")
# this has to happen before -DDEBUG since it wont compile properly with -DDEBUG
add_subdirectory(deps)

# ------------------------ VARIABLES ---------------------------------

include(FindLua)
include(FindOpenSSL)
include(FindThreads)
include(FindZLIB)

set(BeamMP_Sources
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
    include/TPluginMonitor.h src/TPluginMonitor.cpp
    include/Environment.h
)

set(BeamMP_Includes
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/commandline"
    ${LUA_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    "include/tomlplusplus"
    "include/sentry-native/include"
    "include/curl/include"
)

set(BeamMP_Definitions
    SECRET_SENTRY_URL="${BEAMMP_SECRET_SENTRY_URL}"
)

set(BeamMP_Libraries
    doctest::doctest
    OpenSSL::SSL
    OpenSSL::Crypto
    sol2::sol2
    fmt::fmt
    Threads::Threads
    ZLIB::ZLIB
    ${LUA_LIBRARIES}
    commandline
    sentry
    dlhttp
)

if (WIN32)
    set(BeamMP_PlatformLibs wsock32 ws2_32)
endif ()

# ------------------------ BEAMMP SERVER -----------------------------

add_executable(BeamMP-Server
        src/main.cpp
        ${BeamMP_Sources}
)

target_compile_definitions(BeamMP-Server PRIVATE
    ${BeamMP_Definitions}
    DOCTEST_CONFIG_DISABLE
)

target_include_directories(BeamMP-Server PUBLIC 
    ${BeamMP_Includes}
)

target_link_libraries(BeamMP-Server
    ${BeamMP_Libraries}
    ${BeamMP_PlatformLibs}
)

# ------------------------ BEAMMP SERVER TESTS -----------------------

option(BUILD_TESTS "Build BeamMP-Server tests" ON)

if(BUILD_TESTS)
    add_executable(BeamMP-Server-tests
            test/test_main.cpp
            ${BeamMP_Sources}
    )

    target_compile_definitions(BeamMP-Server-tests PRIVATE
        ${BeamMP_Definitions}
    )

    target_include_directories(BeamMP-Server-tests PUBLIC
        ${BeamMP_Includes}
    )

    target_link_libraries(BeamMP-Server-tests
        ${BeamMP_Libraries}
        ${BeamMP_PlatformLibs}
    )
endif()

