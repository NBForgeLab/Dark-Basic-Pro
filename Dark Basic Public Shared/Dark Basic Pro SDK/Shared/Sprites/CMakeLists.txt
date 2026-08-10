add_library(dbp_plugin_sprites STATIC
    CSpriteManagerC.cpp
    CSprites2C.cpp
    CSpritesC.cpp
)

target_include_directories(dbp_plugin_sprites PUBLIC
    "."
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Image"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Setup"
    "${DXSDK_INCLUDE_DIR}"
)

target_compile_definitions(dbp_plugin_sprites PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
)

if (MSVC)
    target_compile_options(dbp_plugin_sprites PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h")
endif()
