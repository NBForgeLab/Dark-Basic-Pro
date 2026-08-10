# DBPPlugins.cmake - Engine SDK Plugins CMake Management Infrastructure

message(STATUS "Configuring Dark Basic Pro Engine SDK Plugins for active architecture...")

# Include Core, Input, Image, and Sound plugin subdirectories
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core" "${CMAKE_BINARY_DIR}/plugins/core")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input" "${CMAKE_BINARY_DIR}/plugins/input")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Image" "${CMAKE_BINARY_DIR}/plugins/image")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Sound" "${CMAKE_BINARY_DIR}/plugins/sound")
