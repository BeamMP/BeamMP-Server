# Modified, original version from https://github.com/filipdutescu/modern-cpp-template (Unlicense)

option(${PROJECT_NAME}_WARNINGS_AS_ERRORS "Treat compiler warnings as errors." OFF)
option(${PROJECT_NAME}_CHECKOUT_GIT_SUBMODULES "If git is found, initialize all submodules." ON)
option(${PROJECT_NAME}_ENABLE_UNIT_TESTING "Enable unit tests for the projects (from the `test` subfolder)." ON)
option(${PROJECT_NAME}_ENABLE_CLANG_TIDY "Enable static analysis with Clang-Tidy." OFF)
option(${PROJECT_NAME}_ENABLE_CPPCHECK "Enable static analysis with Cppcheck." OFF)
# TODO Implement code coverage
# option(${PROJECT_NAME}_ENABLE_CODE_COVERAGE "Enable code coverage through GCC." OFF)
option(${PROJECT_NAME}_ENABLE_DOXYGEN "Enable Doxygen documentation builds of source." OFF)

# Generate compile_commands.json for clang based tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Export all symbols when building a shared library
if(BUILD_SHARED_LIBS)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
endif()

option(${PROJECT_NAME}_ENABLE_LTO "Enable Interprocedural Optimization, aka Link Time Optimization (LTO)." OFF)
if(${PROJECT_NAME}_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT result OUTPUT output)
    if(result)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(SEND_ERROR "IPO is not supported: ${output}.")
    endif()
endif()

option(${PROJECT_NAME}_ENABLE_CCACHE "Enable the usage of Ccache, in order to speed up rebuild times." ON)
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

option(${PROJECT_NAME}_ENABLE_SANITIZER "Enable sanitizer to detect memory errors, undefined behavior, etc. (slows down the executable)." OFF)
if(${PROJECT_NAME}_ENABLE_SANITIZER)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()
