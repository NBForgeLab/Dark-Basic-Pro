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
)

add_library(dbp_plugin_sound STATIC ${SOUND_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_sound)

target_compile_definitions(dbp_plugin_sound PRIVATE
    WIN32
    _WINDOWS
    DIRECTINPUT_VERSION=0x0800
    _MBCS
)

target_include_directories(dbp_plugin_sound PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_sound PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_sound PRIVATE
    dsound
    dxguid
    winmm
    msacm32
    user32
)
