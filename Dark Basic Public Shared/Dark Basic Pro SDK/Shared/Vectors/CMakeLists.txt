project(DBPPluginVectors LANGUAGES C CXX)

set(VECTORS_SOURCES
    cVectorC.cpp
    cVectorManagerC.cpp
)

add_library(dbp_plugin_vectors STATIC ${VECTORS_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_vectors)

target_compile_definitions(dbp_plugin_vectors PRIVATE
    WIN32
    _WINDOWS
    _MBCS
)

target_include_directories(dbp_plugin_vectors PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_vectors PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_vectors PRIVATE
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
)
