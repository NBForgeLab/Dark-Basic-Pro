project(DBPPluginEnhancedMatrix LANGUAGES C CXX)

set(ENHANCEDMATRIX_SOURCES
    CEnhancedMatrixC.cpp
    CEnhancedMatrixManagerC.cpp
    Modes/CModeC.cpp
    Modes/QuadTrees/CQuadTreesC.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Position/CPositionC.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Data/CDataC.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/AdvancedMatrix/AdvancedMatrix.rc"
)

add_library(dbp_plugin_enhancedmatrix SHARED ${ENHANCEDMATRIX_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_enhancedmatrix)

set_target_properties(dbp_plugin_enhancedmatrix PROPERTIES
    OUTPUT_NAME "DBProAdvancedMatrixDebug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_compile_definitions(dbp_plugin_enhancedmatrix PRIVATE
    WIN32
    _WINDOWS
    _MBCS
    _CRT_SECURE_NO_WARNINGS
)

target_include_directories(dbp_plugin_enhancedmatrix PUBLIC
    .
    Modes
    Modes/QuadTrees
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Data
    ${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Position
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_enhancedmatrix PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_enhancedmatrix PRIVATE
    winmm
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
