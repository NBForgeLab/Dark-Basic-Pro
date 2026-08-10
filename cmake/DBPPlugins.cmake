# DBPPlugins.cmake - Engine SDK Plugins CMake Management Infrastructure

message(STATUS "Configuring Dark Basic Pro Engine SDK Plugins for active architecture...")

# Include Core, Input, Image, Sound, Camera, and Text plugin subdirectories
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core" "${CMAKE_BINARY_DIR}/plugins/core")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input" "${CMAKE_BINARY_DIR}/plugins/input")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Image" "${CMAKE_BINARY_DIR}/plugins/image")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Sound" "${CMAKE_BINARY_DIR}/plugins/sound")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Camera" "${CMAKE_BINARY_DIR}/plugins/camera")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Text" "${CMAKE_BINARY_DIR}/plugins/text")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Bitmap" "${CMAKE_BINARY_DIR}/plugins/bitmap")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/File" "${CMAKE_BINARY_DIR}/plugins/file")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/FTP" "${CMAKE_BINARY_DIR}/plugins/ftp")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Matrix" "${CMAKE_BINARY_DIR}/plugins/matrix")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Setup" "${CMAKE_BINARY_DIR}/plugins/setup")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Memblocks" "${CMAKE_BINARY_DIR}/plugins/memblocks")
