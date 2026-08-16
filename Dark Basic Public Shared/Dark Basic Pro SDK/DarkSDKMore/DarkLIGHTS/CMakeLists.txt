project(DBPPluginLightMapper LANGUAGES C CXX)

set(LIGHTMAPPER_SOURCES
    CollisionTreeLightmapper.cpp
    Light.cpp
    LightMapper.cpp
    LMGlobal.cpp
    LMObject.cpp
    LMPoly.cpp
    LMPolyGroup.cpp
    LMTexNode.cpp
    LMTexture.cpp
    Lumel.cpp
    SharedData.cpp
    TreeFaceLightmapper.cpp
    LightMapper.rc
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
)

add_library(dbp_plugin_lightmapper SHARED ${LIGHTMAPPER_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_lightmapper)

set_target_properties(dbp_plugin_lightmapper PROPERTIES
    OUTPUT_NAME "LightMapper"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
)

target_compile_definitions(dbp_plugin_lightmapper PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
    LIGHTMAPPER_EXPORTS
    _MBCS
    _CRT_SECURE_NO_WARNINGS
)

target_include_directories(dbp_plugin_lightmapper PUBLIC
    .
    "${CMAKE_SOURCE_DIR}/SDK/BaseClasses"
    "${CMAKE_SOURCE_DIR}/SDK/DB3"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Include"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Camera"
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_lightmapper PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_lightmapper PRIVATE
    winmm
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
