cmake_minimum_required(VERSION 3.0)
project(watchdog CXX ASM_MASM)

set(CMAKE_CXX_STANDARD 17)
add_library(watchdog STATIC watchdog.cpp watchdog.h x64Def.asm)
set_source_files_properties(watchdog.cpp PROPERTIES COMPILE_FLAGS "/O2 /Ob2 /DNDEBUG")
STRING(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
target_link_libraries(watchdog Dbghelp)