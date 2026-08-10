add_library(dbp_plugin_music STATIC
    BasePlayer.cpp
    CMusicC.cpp
    CMusicManagerC.cpp
    CdPlayer.cpp
    MidiPlayer.cpp
    MusicPlayer.cpp
)

target_include_directories(dbp_plugin_music PUBLIC
    "."
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/BaseClasses"
    "${DXSDK_INCLUDE_DIR}"
)

target_compile_definitions(dbp_plugin_music PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(dbp_plugin_music PRIVATE
    winmm.lib
    strmiids.lib
    quartz.lib
)

if (MSVC)
    target_compile_options(dbp_plugin_music PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h")
endif()
