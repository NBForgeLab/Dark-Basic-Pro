project(DBPPluginAdvancedTerrain LANGUAGES C CXX)

set(BLITZTERRAIN_SOURCES
    code-from-dbp.cpp
    CreateObject.cpp
    EnvironmentMapping.cpp
    main.cpp
    quadmapping.cpp
    rttms.cpp
    Resource.rc
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
)

add_library(dbp_plugin_advancedterrain SHARED ${BLITZTERRAIN_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_advancedterrain)

set_target_properties(dbp_plugin_advancedterrain PROPERTIES
    OUTPUT_NAME "AdvancedTerrain"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_compile_definitions(dbp_plugin_advancedterrain PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
    BLITZTERRAIN_EXPORTS
    DBP_PLUGIN_CONSUMER
    _MBCS
    _CRT_SECURE_NO_WARNINGS
)

target_include_directories(dbp_plugin_advancedterrain PUBLIC
    .
    "${CMAKE_SOURCE_DIR}/SDK/BaseClasses"
    "${CMAKE_SOURCE_DIR}/SDK/DB3"
    "${CMAKE_SOURCE_DIR}/SDK/NVAPI"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Include"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Camera"
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_advancedterrain PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_advancedterrain PRIVATE
    dbp_plugin_core
    dbp_plugin_objects
    dbp_plugin_memblocks
    dbp_plugin_camera
    dbp_plugin_image
    winmm
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
