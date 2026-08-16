project(DBPPluginConvX LANGUAGES C CXX)

set(CONVX_SOURCES
    ConvX.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
)

add_library(dbp_plugin_convx SHARED ${CONVX_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_convx)

set_target_properties(dbp_plugin_convx PROPERTIES
    OUTPUT_NAME "convx"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
)

target_include_directories(dbp_plugin_convx PUBLIC
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

target_compile_definitions(dbp_plugin_convx PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    WIN32
    _WINDOWS
    _MBCS
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_convx PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_convx PRIVATE
    dbp_plugin_dboformat
    winmm
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
