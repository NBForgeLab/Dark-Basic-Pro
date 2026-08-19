project(DBPPluginAnimation LANGUAGES C CXX)

set(ANIMATION_SOURCES
    CAnimation.cpp
    CTextureRenderer.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/Animation/Animation.rc"
)

add_library(dbp_plugin_animation SHARED ${ANIMATION_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_animation)

set_target_properties(dbp_plugin_animation PROPERTIES
    OUTPUT_NAME "DBProAnimationDebug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_include_directories(dbp_plugin_animation PUBLIC
    "."
    "${CMAKE_SOURCE_DIR}/SDK/BaseClasses"
    "${CMAKE_SOURCE_DIR}/SDK/DB3"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Image"
    "${DXSDK_INCLUDE_DIR}"
)

target_compile_definitions(dbp_plugin_animation PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    WIN32
    _WINDOWS
    _MBCS
)

if (MSVC)
    target_compile_options(dbp_plugin_animation PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h")
endif()

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_animation PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_animation PRIVATE
    strmiids
    winmm
    ole32
    oleaut32
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
