project(DBPPluginFile LANGUAGES C CXX)

set(FILE_SOURCES
    CFileC.cpp
)

add_library(dbp_plugin_file STATIC ${FILE_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_file)

target_compile_definitions(dbp_plugin_file PRIVATE
    WIN32
    _WINDOWS
    _MBCS
)

target_include_directories(dbp_plugin_file PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${DXSDK_INCLUDE_DIR}
)

target_link_libraries(dbp_plugin_file PRIVATE
    user32
)
