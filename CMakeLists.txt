cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

if (WIN32)
    set(VCPKG_TARGET_TRIPLET x64-windows-static)
endif()

include(cmake/Vcpkg.cmake) # needs to happen before project()

project(
    "BeamMP-Server" # replace this
    VERSION 3.3.0
)

include(cmake/StandardSettings.cmake)
include(cmake/StaticAnalyzers.cmake)
include(cmake/Git.cmake)

# below are options which should be changed

### SETTINGS ###

# add all headers (.h, .hpp) to this
set(PRJ_HEADERS 
    include/ArgsParser.h
    include/BoostAliases.h
    include/Client.h
    include/Common.h
    include/Compat.h
    include/Cryptography.h
    include/CustomAssert.h
    include/Defer.h
    include/Environment.h
    include/Http.h
    include/IThreaded.h
    include/Json.h
    include/LuaAPI.h
    include/RWMutex.h
    include/SignalHandling.h
    include/TConfig.h
    include/TConsole.h
    include/THeartbeatThread.h
    include/TLuaEngine.h
    include/TLuaPlugin.h
    include/TNetwork.h
    include/TPluginMonitor.h
    include/TPPSMonitor.h
    include/TResourceManager.h
    include/TScopedTimer.h
    include/TServer.h
    include/VehicleData.h
    include/Env.h
    include/Profiling.h
)
# add all source files (.cpp) to this, except the one with main()
set(PRJ_SOURCES
    src/ArgsParser.cpp
    src/Client.cpp
    src/Common.cpp
    src/Compat.cpp
    src/Http.cpp
    src/LuaAPI.cpp
    src/SignalHandling.cpp
    src/TConfig.cpp
    src/TConsole.cpp
    src/THeartbeatThread.cpp
    src/TLuaEngine.cpp
    src/TLuaPlugin.cpp
    src/TNetwork.cpp
    src/TPluginMonitor.cpp
    src/TPPSMonitor.cpp
    src/TResourceManager.cpp
    src/TScopedTimer.cpp
    src/TServer.cpp
    src/VehicleData.cpp
    src/Env.cpp
    src/Profiling.cpp
)

find_package(Lua REQUIRED)

# set the source file containing main()
set(PRJ_MAIN src/main.cpp)
# set the source file containing the test's main
set(PRJ_TEST_MAIN test/test_main.cpp)
# set include paths not part of libraries
set(PRJ_INCLUDE_DIRS ${LUA_INCLUDE_DIR})
# set compile features (e.g. standard version)
set(PRJ_COMPILE_FEATURES cxx_std_20)
# set #defines (test enable/disable not included here)
set(PRJ_DEFINITIONS CPPHTTPLIB_OPENSSL_SUPPORT)
# add all libraries used by the project (WARNING: also set them in vcpkg.json!)
set(PRJ_LIBRARIES 
    fmt::fmt
    doctest::doctest
    Threads::Threads
    commandline_static
    toml11::toml11
    rapidjson
    sol2
    httplib::httplib
    libzip::zip
    OpenSSL::SSL OpenSSL::Crypto
    ${LUA_LIBRARIES}
)

# add dependency find_package calls and similar here
find_package(fmt CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(doctest CONFIG REQUIRED)
find_package(Boost REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(toml11 CONFIG REQUIRED)

include_directories(include)

# to enable multithreading and the Threads::Threads dependency
include(FindThreads)

### END SETTINGS ###

# DONT change anything beyond this point unless you've read the cmake bible and 
# swore on it not to bonk up the ci/cd pipelines with your changes.

####################


# enables compile_commands.json for clang-related tools (such as the clang LS)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# build release builds by default (if not specified otherwise)
if(NOT DEFINED CMAKE_BUILD_TYPE)
    message(NOTICE "No build type specified, defaulting to 'Release'")
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(UNIX)
    # this will allow to use same _DEBUG macro available in both Linux as well as Windows - MSCV environment. Easy to put Debug specific code.
    add_compile_options("$<$<CONFIG:DEBUG>:-D_DEBUG>")
endif(UNIX)

if (WIN32)
    add_compile_options("-D_WIN32_WINNT=0x0601")
    add_compile_options("/bigobj")
endif(WIN32)


include(cmake/CompilerWarnings.cmake)

# set MT library for msvc - this is required (says documentation)
# linux/mac/etc should simply ignore this by default.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(PRJ_DEFINITIONS ${PRJ_DEFINITIONS}
    PRJ_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    PRJ_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    PRJ_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# build commandline manually for funky windows flags to carry over without a custom toolchain file
add_library(commandline_static 
    deps/commandline/src/impls.h
    deps/commandline/src/windows_impl.cpp
    deps/commandline/src/linux_impl.cpp
    deps/commandline/src/backends/InteractiveBackend.cpp
    deps/commandline/src/backends/InteractiveBackend.h
    deps/commandline/src/backends/Backend.cpp
    deps/commandline/src/backends/Backend.h
    deps/commandline/src/commandline.h
    deps/commandline/src/commandline.cpp
    deps/commandline/src/backends/BufferedBackend.cpp
    deps/commandline/src/backends/BufferedBackend.h
)
if (WIN32)
    target_compile_definitions(commandline_static PRIVATE -DPLATFORM_WINDOWS=1)
else ()
    target_compile_definitions(commandline_static PRIVATE -DPLATFORM_LINUX=1)
endif ()
target_include_directories(commandline_static PUBLIC "deps/commandline/src")
target_link_libraries(commandline_static Threads::Threads)
# end of commandline custom build

add_executable(${PROJECT_NAME} ${PRJ_HEADERS} ${PRJ_SOURCES} ${PRJ_MAIN})
set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)
target_include_directories(${PROJECT_NAME} PRIVATE ${PRJ_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${PRJ_LIBRARIES})
target_compile_features(${PROJECT_NAME} PRIVATE ${PRJ_COMPILE_FEATURES})
target_compile_definitions(${PROJECT_NAME} PRIVATE ${PRJ_DEFINITIONS} ${PRJ_WARNINGS}
    DOCTEST_CONFIG_DISABLE # disables all test code in the final executable
)

if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE "/SUBSYSTEM:CONSOLE")
endif(MSVC)

# setup all warnings (from cmake/CompilerWarnings.cmake)
set_project_warnings(${PROJECT_NAME})

if(${PROJECT_NAME}_ENABLE_UNIT_TESTING)
    message(STATUS "Unit tests are enabled and will be built as '${PROJECT_NAME}-tests'")
    add_executable(${PROJECT_NAME}-tests ${PRJ_HEADERS} ${PRJ_SOURCES} ${PRJ_TEST_MAIN})
    target_include_directories(${PROJECT_NAME}-tests PRIVATE ${PRJ_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME}-tests ${PRJ_LIBRARIES})
    target_compile_features(${PROJECT_NAME}-tests PRIVATE ${PRJ_COMPILE_FEATURES})
    target_compile_definitions(${PROJECT_NAME}-tests PRIVATE ${PRJ_DEFINITIONS} ${PRJ_WARNINGS})
    set_project_warnings(${PROJECT_NAME}-tests)
    if(MSVC)
        target_link_options(${PROJECT_NAME}-tests PRIVATE "/SUBSYSTEM:CONSOLE")
    endif(MSVC)
endif()

