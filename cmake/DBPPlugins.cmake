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
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat" "${CMAKE_BINARY_DIR}/plugins/dboformat")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Objects" "${CMAKE_BINARY_DIR}/plugins/objects")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Light" "${CMAKE_BINARY_DIR}/plugins/light")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/SpecialEffects" "${CMAKE_BINARY_DIR}/plugins/specialeffects")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Vectors" "${CMAKE_BINARY_DIR}/plugins/vectors")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Transforms" "${CMAKE_BINARY_DIR}/plugins/transforms")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Q2BSP" "${CMAKE_BINARY_DIR}/plugins/q2bsp")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Q3BSP" "${CMAKE_BINARY_DIR}/plugins/q3bsp")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/CustomBSP" "${CMAKE_BINARY_DIR}/plugins/custombsp")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Sprites" "${CMAKE_BINARY_DIR}/plugins/sprites")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Basic2D" "${CMAKE_BINARY_DIR}/plugins/basic2d")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Animation" "${CMAKE_BINARY_DIR}/plugins/animation")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Music" "${CMAKE_BINARY_DIR}/plugins/music")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System" "${CMAKE_BINARY_DIR}/plugins/system")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Data" "${CMAKE_BINARY_DIR}/plugins/data")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/ConvX" "${CMAKE_BINARY_DIR}/plugins/convx")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Conv3DS" "${CMAKE_BINARY_DIR}/plugins/conv3ds")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/ConvMD2" "${CMAKE_BINARY_DIR}/plugins/convmd2")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/ConvMD3" "${CMAKE_BINARY_DIR}/plugins/convmd3")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/ConvMDL" "${CMAKE_BINARY_DIR}/plugins/convmdl")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/EnhancedMatrix" "${CMAKE_BINARY_DIR}/plugins/enhancedmatrix")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/GameFX" "${CMAKE_BINARY_DIR}/plugins/gamefx")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/ODE" "${CMAKE_BINARY_DIR}/plugins/ode")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/BlitzTerrain" "${CMAKE_BINARY_DIR}/plugins/blitzterrain")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/DarkLIGHTS" "${CMAKE_BINARY_DIR}/plugins/darklights")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/DarkAI" "${CMAKE_BINARY_DIR}/plugins/darkai")
add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDKMore/Enhancements" "${CMAKE_BINARY_DIR}/plugins/enhancements")

# Multiplayer and MultiplayerPlus are excluded: they depend on the deprecated
# DirectPlay4/8 SDK headers (dplay.h, dplobby.h, dplay8.h, dpaddr.h) which are
# not available in modern Windows SDKs or the Microsoft.DXSDK.D3DX NuGet package.
# Re-enable when the legacy June 2010 DirectX SDK is installed or shim headers
# are provided.
# add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Multiplayer" "${CMAKE_BINARY_DIR}/plugins/multiplayer")
# add_subdirectory("${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/MultiplayerPlus" "${CMAKE_BINARY_DIR}/plugins/multiplayerplus")


