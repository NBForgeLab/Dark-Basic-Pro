# DBPPlugins.cmake - Engine SDK Plugins CMake Management Infrastructure

message(STATUS "Configuring Dark Basic Pro Engine SDK Plugins for active architecture...")

# Include Core and Input plugin subdirectories
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core" "${CMAKE_BINARY_DIR}/plugins/core")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input" "${CMAKE_BINARY_DIR}/plugins/input")
