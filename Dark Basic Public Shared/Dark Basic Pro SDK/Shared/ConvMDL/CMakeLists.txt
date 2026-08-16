project(DBPPluginConvMDL LANGUAGES C CXX)

set(CONVMDL_SOURCES
    ConvMDL.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
)

add_library(dbp_plugin_convmdl SHARED ${CONVMDL_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_convmdl)

set_target_properties(dbp_plugin_convmdl PROPERTIES
    OUTPUT_NAME "ConvMDL"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
)

target_include_directories(dbp_plugin_convmdl PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Include
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat
    ${DXSDK_INCLUDE_DIR}
)

target_compile_definitions(dbp_plugin_convmdl PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    WIN32
    _WINDOWS
    _MBCS
    _HAS_STD_BYTE=0
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_convmdl PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_convmdl PRIVATE
    dbp_plugin_dboformat
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
