project(DBPPluginSound LANGUAGES C CXX)

set(SOUND_SOURCES
    ADPCMtoWAV.cpp
    CSoundC.cpp
    CSoundManagerC.cpp
    OggVorbis.cpp
    OggVorbisEncoderAudio.cpp
    OggVorbisEncoderEncode.cpp
    OggVorbisEncoderMain.cpp
    OggVorbisEncoderPlatform.cpp
    dsutil.cpp
    dxutil.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Data/CDataC.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/SteamCheckForWorkshop.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/Sound/Sound.rc"
)

add_library(dbp_plugin_sound SHARED ${SOUND_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_sound)

set_target_properties(dbp_plugin_sound PROPERTIES
    OUTPUT_NAME "DBProSoundDebug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_compile_definitions(dbp_plugin_sound PRIVATE
    WIN32
    _WINDOWS
    DIRECTINPUT_VERSION=0x0800
    _MBCS
    _CRT_SECURE_NO_WARNINGS
)

target_include_directories(dbp_plugin_sound PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Data
    ${DXSDK_INCLUDE_DIR}
)

target_link_directories(dbp_plugin_sound PRIVATE
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Sound/vorbis/lib"
    "${CMAKE_SOURCE_DIR}/SDK/OGG/lib64/Release"
)
if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_sound PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_sound PRIVATE
    ogg_static
    vorbis_static
    vorbisfile_static
    dsound
    dxguid
    winmm
    msacm32
    user32
    gdi32
    advapi32
)
