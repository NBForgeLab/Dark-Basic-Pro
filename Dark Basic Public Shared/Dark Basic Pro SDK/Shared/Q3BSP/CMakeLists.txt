add_library(dbp_plugin_q3bsp STATIC
    CBSPFile.cpp
    Collision.cpp
    Loader.cpp
    Patch.cpp
    Shaders.cpp
    Textures.cpp
    Unzip.cpp
)

target_include_directories(dbp_plugin_q3bsp PUBLIC
    .
    col
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat
    ${DXSDK_INCLUDE_DIR}
)

target_compile_definitions(dbp_plugin_q3bsp PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(dbp_plugin_q3bsp PRIVATE
    winmm.lib
)

if (MSVC)
    target_compile_options(dbp_plugin_q3bsp PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h" /FImmsystem.h /Zc:forScope-)
endif()
