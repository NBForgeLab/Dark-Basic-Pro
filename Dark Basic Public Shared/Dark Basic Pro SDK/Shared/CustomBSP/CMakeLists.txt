project(DBPPluginCustomBSP LANGUAGES C CXX)

set(CUSTOMBSP_SOURCES
    CCollisionManager.cpp
    CCustomBSPC.cpp
    Collision.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/CustomBSP/CustomBSP.rc"
)

add_library(dbp_plugin_custombsp SHARED ${CUSTOMBSP_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_custombsp)

set_target_properties(dbp_plugin_custombsp PROPERTIES
    OUTPUT_NAME "DBProCustomBSPDebug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_include_directories(dbp_plugin_custombsp PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat
    ${DXSDK_INCLUDE_DIR}
)

target_compile_definitions(dbp_plugin_custombsp PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    WIN32
    _WINDOWS
    _MBCS
    _HAS_STD_BYTE=0
)

if (MSVC)
    target_compile_options(dbp_plugin_custombsp PRIVATE /Zc:forScope- /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h")
endif()

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_custombsp PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_custombsp PRIVATE
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
