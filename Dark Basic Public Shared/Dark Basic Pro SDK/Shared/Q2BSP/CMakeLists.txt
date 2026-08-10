add_library(dbp_plugin_q2bsp STATIC
    CQ2BSP.cpp
    Collision.cpp
    Loader.cpp
    Textures.cpp
    Unzip.cpp
)

target_include_directories(dbp_plugin_q2bsp PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat
    ${DXSDK_INCLUDE_DIR}
)

target_compile_definitions(dbp_plugin_q2bsp PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
)

if (MSVC)
    target_compile_options(dbp_plugin_q2bsp PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h" /Zc:forScope-)
endif()
