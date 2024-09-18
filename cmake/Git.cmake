find_package(Git)
if(${PROJECT_NAME}_CHECKOUT_GIT_SUBMODULES)
    if(Git_FOUND)
        message(STATUS "Git found, submodule update and init")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(SEND_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules. This may result in missing dependencies.")
        endif()
    else()
        message(SEND_ERROR "git required for checking out submodules, but not found. Submodules will not be checked out - this may result in missing dependencies.")
    endif()
endif()

if(Git_FOUND)

else()
    message(STATUS "Git not found - the version will not include a git hash.")
    set(PRJ_GIT_HASH "unknown")
endif()
