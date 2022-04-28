include(FetchContent)

message(STATUS "Getting, checking and running vcpkg, this may take a while")

FetchContent_Declare(
  vcpkg
  GIT_REPOSITORY    https://github.com/microsoft/vcpkg.git
)

FetchContent_GetProperties(vcpkg)

if(NOT vcpkg_POPULATED)
  FetchContent_Populate(vcpkg)
  execute_process(COMMAND ./${vcpkg_SOURCE_DIR}/bootstrap-vcpkg.sh)
endif()

set(CMAKE_TOOLCHAIN_FILE ${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake
CACHE STRING "Vcpkg toolchain file")
