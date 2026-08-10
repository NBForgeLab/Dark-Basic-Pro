project(DBPPluginObjects LANGUAGES C CXX)

set(OBJECTS_SOURCES
    BSPTree.cpp
    CObjectManagerC.cpp
    CObjectsC.cpp
    CPositionC.cpp
    CSG.cpp
    Collision.cpp
    CommonC.cpp
    GameFXCalls.cpp
    Nodetree.cpp
    Universe.cpp
    cFrustum.cpp
    cLightMaps.cpp
    cNodeTree.cpp
)

add_library(dbp_plugin_objects STATIC ${OBJECTS_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_objects)

target_compile_definitions(dbp_plugin_objects PRIVATE
    WIN32
    _WINDOWS
    _MBCS
)

target_include_directories(dbp_plugin_objects PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/SDK/NVAPI
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_objects PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_objects PRIVATE
    dbp_plugin_dboformat
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
)
