# DBPPlugins.cmake - Engine SDK Plugins CMake Management Infrastructure

message(STATUS "Configuring Dark Basic Pro Engine SDK Plugins for active architecture...")

# Include Input plugin subdirectory
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input" "${CMAKE_BINARY_DIR}/plugins/input")
