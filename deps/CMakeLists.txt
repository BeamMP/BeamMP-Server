include_directories("${PROJECT_SOURCE_DIR}/deps")
include_directories("${PROJECT_SOURCE_DIR}/deps/commandline")

add_subdirectory("${PROJECT_SOURCE_DIR}/deps/commandline")
add_subdirectory("${PROJECT_SOURCE_DIR}/deps/fmt")
add_subdirectory("${PROJECT_SOURCE_DIR}/deps/sol2")
add_subdirectory("${PROJECT_SOURCE_DIR}/deps/doctest")
