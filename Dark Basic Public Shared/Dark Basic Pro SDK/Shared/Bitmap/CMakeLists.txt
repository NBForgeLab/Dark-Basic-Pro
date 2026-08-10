project(DBPPluginBitmap LANGUAGES C CXX)

set(BITMAP_SOURCES
    CBitmapC.cpp
)

add_library(dbp_plugin_bitmap STATIC ${BITMAP_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_bitmap)

target_compile_definitions(dbp_plugin_bitmap PRIVATE
    WIN32
    _WINDOWS
    _MBCS
)

target_include_directories(dbp_plugin_bitmap PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${DXSDK_INCLUDE_DIR}
)

target_link_libraries(dbp_plugin_bitmap PRIVATE
    gdi32
    user32
)
