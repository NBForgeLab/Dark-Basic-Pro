project(DBPPluginSprites LANGUAGES C CXX)

set(SPRITES_SOURCES
    CSpriteManagerC.cpp
    CSprites2C.cpp
    CSpritesC.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/SteamCheckForWorkshop.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/Sprites/Sprites.rc"
)

add_library(dbp_plugin_sprites SHARED ${SPRITES_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_sprites)

set_target_properties(dbp_plugin_sprites PROPERTIES
    OUTPUT_NAME "DBProSpritesDebug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_include_directories(dbp_plugin_sprites PUBLIC
    "."
    "${CMAKE_SOURCE_DIR}/SDK/BaseClasses"
    "${CMAKE_SOURCE_DIR}/SDK/DB3"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Image"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Setup"
    "${DXSDK_INCLUDE_DIR}"
)

target_compile_definitions(dbp_plugin_sprites PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    WIN32
    _WINDOWS
    _MBCS
)

if (MSVC)
    target_compile_options(dbp_plugin_sprites PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h")
endif()

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_sprites PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_sprites PRIVATE
    winmm
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
