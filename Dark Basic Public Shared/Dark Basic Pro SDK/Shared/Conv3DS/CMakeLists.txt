project(DBPPluginConv3DS LANGUAGES C CXX)

set(CONV3DS_SOURCES
    Conv3DS.cpp
    d3dmath.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
)

add_library(dbp_plugin_conv3ds SHARED ${CONV3DS_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_conv3ds)

set_target_properties(dbp_plugin_conv3ds PROPERTIES
    OUTPUT_NAME "Conv3DS"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
)

target_include_directories(dbp_plugin_conv3ds PUBLIC
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

target_compile_definitions(dbp_plugin_conv3ds PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    WIN32
    _WINDOWS
    _MBCS
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_conv3ds PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_conv3ds PRIVATE
    dbp_plugin_dboformat
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
