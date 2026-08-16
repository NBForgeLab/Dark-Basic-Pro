project(DBPPluginMusic LANGUAGES C CXX)

set(MUSIC_SOURCES
    BasePlayer.cpp
    CMusicC.cpp
    CMusicManagerC.cpp
    CdPlayer.cpp
    MidiPlayer.cpp
    MusicPlayer.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/SteamCheckForWorkshop.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/Music/Music.rc"
)

add_library(dbp_plugin_music SHARED ${MUSIC_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_music)

set_target_properties(dbp_plugin_music PROPERTIES
    OUTPUT_NAME "DBProMusicDebug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
)

target_include_directories(dbp_plugin_music PUBLIC
    "."
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/BaseClasses"
    "${DXSDK_INCLUDE_DIR}"
)

target_compile_definitions(dbp_plugin_music PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    WIN32
    _WINDOWS
    _MBCS
)

target_link_libraries(dbp_plugin_music PRIVATE
    winmm
    strmiids
    quartz
    user32
    gdi32
    advapi32
)

if (MSVC)
    target_compile_options(dbp_plugin_music PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h")
endif()
