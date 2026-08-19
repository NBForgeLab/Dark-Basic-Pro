project(DBPPluginObjects LANGUAGES C CXX)

set(OBJECTS_SOURCES
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
    BoxCollision/3DCol.cpp
    BoxCollision/cBoxCol.cpp
    BoxCollision/CCollision.cpp
    BoxCollision/PolyPool.cpp
    Compiler/CBounds.cpp
    Compiler/CBSPTree.cpp
    Compiler/CCompiler.cpp
    Compiler/CompilerTypes.cpp
    Compiler/CPlane.cpp
    Compiler/CPortals.cpp
    Compiler/CVector.cpp
    Compiler/ProcessHSR.cpp
    ElipsoidCollision/col_response.cpp
    ElipsoidCollision/col_step.cpp
    ElipsoidCollision/col_types.cpp
    ElipsoidCollision/Collision.cpp
    Occlusion/cOcclusion.cpp
    Occlusion/HZBManager.cpp
    ShadowMapping/cShadowMaps.cpp
    ShadowMapping/DepthTexture.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/Objects/ObjectsPlus.rc"
)

add_library(dbp_plugin_objects SHARED ${OBJECTS_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_objects)

set_target_properties(dbp_plugin_objects PROPERTIES
    OUTPUT_NAME "DBProBasic3DDebug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_compile_definitions(dbp_plugin_objects PRIVATE
    WIN32
    _WINDOWS
    _MBCS
    _CRT_SECURE_NO_WARNINGS
    SKIPD3DHACKS
)

target_include_directories(dbp_plugin_objects PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/SDK/NVAPI
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_objects PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_objects PRIVATE
    dbp_plugin_dboformat
    winmm
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
