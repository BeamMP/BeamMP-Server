# 3.4 is required for imported targets.
cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

message(STATUS "You can find build instructions and a list of dependencies in the README at \
https://github.com/BeamMP/BeamMP-Server")

include(FetchContent)
FetchContent_Declare(
  cmake-tools
  GIT_REPOSITORY https://github.com/brobeson/cmake-tools.git
  GIT_TAG main
)
FetchContent_MakeAvailable(cmake-tools)
list(APPEND CMAKE_MODULE_PATH "${cmake-tools_SOURCE_DIR}")

include(CMakeToolsVersionFromGit)

execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )

project(BeamMP-Server
    DESCRIPTION "Server for BeamMP - The Multiplayer Mod for BeamNG.drive"
    HOMEPAGE_URL https://beammp.com
    LANGUAGES CXX C)

# quality of life stuff
set(CMAKE_COLOR_DIAGNOSTICS ON)

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

add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT=1)

option(WIN32_STATIC_RUNTIME "Build statically-linked runtime on windows (don't touch unless you know what you're doing)" ON)

# ------------------------ APPLE ---------------------------------
if(APPLE)
    if(IS_DIRECTORY /opt/homebrew/Cellar/lua@5.3/5.3.6)
        set(LUA_INCLUDE_DIR /opt/homebrew/Cellar/lua@5.3/5.3.6/include/lua5.3)
        link_directories(/opt/homebrew/Cellar/lua@5.3/5.3.6/lib)
    else()
        set(LUA_INCLUDE_DIR /usr/local/Cellar/lua@5.3/5.3.6/include/lua5.3)
        link_directories(/usr/local/Cellar/lua@5.3/5.3.6/lib)
    endif()
    set(LUA_LIBRARIES lua)
    if(IS_DIRECTORY /opt/homebrew/opt/openssl@1.1)
        include_directories(/opt/homebrew/opt/openssl@1.1/include)
        link_directories(/opt/homebrew/opt/openssl@1.1/lib)
    else()
        include_directories(/usr/local/opt/openssl@1.1/include)
        link_directories(/usr/local/opt/openssl@1.1/lib)
    endif()
# ------------------------ WINDOWS ---------------------------------
elseif (WIN32)
    if (WIN32_STATIC_RUNTIME)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
# ------------------------ UNIX ------------------------------------
elseif (UNIX)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
    option(SANITIZE "Turns on thread and UB sanitizers" OFF)
    if (SANITIZE)
        message(STATUS "sanitize is ON")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize={address,thread,undefined}")
    endif (SANITIZE)
endif ()

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
set(CMAKE_CXX_STANDARD 20)

# ------------------------ DEPENDENCIES ------------------------------
message(STATUS "Adding local source dependencies")
# this has to happen before -DDEBUG since it wont compile properly with -DDEBUG
add_subdirectory(deps)

# ------------------------ VARIABLES ---------------------------------

include(FindLua)
include(FindOpenSSL)
include(FindThreads)
include(FindZLIB)

find_package(Boost 1.70 REQUIRED COMPONENTS system)

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
    include/BoostAliases.h
    include/Uuid.h src/Uuid.cpp
)

set(BeamMP_Includes
    ${LUA_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/cpp-httplib"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/commandline"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/json/single_include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/sol2/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/rapidjson/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/asio/asio/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/lk-result/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/fmt/include"
)

set(BeamMP_Definitions
    SECRET_SENTRY_URL="${BEAMMP_SECRET_SENTRY_URL}"
    BEAMMP_GIT_HASH="${GIT_HASH}"
    LK_RESULT_USE_FMT
)

if (WIN32)
    list(APPEND BeamMP_Definitions _WIN32_WINNT=0x0601)
    list(APPEND BeamMP_Definitions _CRT_SECURE_NO_WARNINGS)
endif()
if (UNIX)
    set(BeamMP_CompileOptions
        -Wall
        -Wextra
        -Wpedantic
    
        -Werror=uninitialized
        -Werror=float-equal
        -Werror=pointer-arith
        -Werror=double-promotion
        -Werror=write-strings
        -Werror=cast-qual
        -Werror=init-self
        -Werror=cast-align
        -Werror=unreachable-code
        -Werror=strict-aliasing -fstrict-aliasing
        -Werror=redundant-decls
        -Werror=missing-declarations
        -Werror=missing-field-initializers
        -Werror=write-strings
        -Werror=ctor-dtor-privacy
        -Wswitch-enum
        -Wswitch-default
        -Werror=old-style-cast
        -Werror=overloaded-virtual
        -Werror=overloaded-virtual
        -Werror=missing-include-dirs
        -Werror=unused-result

        -fstack-protector
        -Wzero-as-null-pointer-constant
    )
else()
    set(BeamMP_CompileOptions
        /bigobj
        /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT /NODEFAULTLIB:LIBCMT
    )
endif()

set(BeamMP_Libraries
    Boost::boost
    Boost::system
    doctest::doctest
    OpenSSL::SSL
    OpenSSL::Crypto
    sol2::sol2
    Threads::Threads
    ZLIB::ZLIB
    ${LUA_LIBRARIES}
    commandline
    sentry
    fmt::fmt
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

target_compile_options(BeamMP-Server PRIVATE 
    ${BeamMP_CompileOptions}
)

target_include_directories(BeamMP-Server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_include_directories(BeamMP-Server SYSTEM PRIVATE
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
    
    target_compile_options(BeamMP-Server-tests PRIVATE 
       ${BeamMP_CompileOptions}
    )

    target_include_directories(BeamMP-Server-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_include_directories(BeamMP-Server-tests SYSTEM PRIVATE
        ${BeamMP_Includes}
    )
    
    target_link_libraries(BeamMP-Server-tests
        ${BeamMP_Libraries}
        ${BeamMP_PlatformLibs}
    )
endif()

