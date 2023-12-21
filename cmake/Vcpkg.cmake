if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)
        find_package(Git)
        if(Git_FOUND)
            execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive vcpkg
                            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                            RESULT_VARIABLE GIT_SUBMOD_RESULT)
            if(NOT GIT_SUBMOD_RESULT EQUAL "0")
                message(SEND_ERROR "Checking out vcpkg in source tree failed with ${GIT_SUBMOD_RESULT}.")
            endif()
        else()
            message(FATAL_ERROR "Could not find git or vcpkg.cmake. Please either, install git and re-run cmake (or run `git submodule update --init --recursive`), or install vcpkg and add `-DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake` to your cmake invocation. Please try again after making those changes.")
        endif()
    endif()
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)
endif()

