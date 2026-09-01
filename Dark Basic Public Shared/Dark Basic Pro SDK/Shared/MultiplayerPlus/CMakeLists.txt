# MultiplayerPlus plugin (DirectPlay8 TCP) built as a real DLL.
# The legacy DirectPlay8 SDK headers are vendored under ThirdParty/DirectPlay8.
# GUIDs come from MultiplayerGuids.cpp (initguid.h) instead of dxguid.lib.

project(DBPPluginMultiplayerPlus LANGUAGES C CXX)

set(MPPLUS_SOURCES
    Multiplayer.cpp
    MultiplayerGuids.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/MultiplayerPlus/Script1.rc"
)

add_library(dbp_plugin_multiplayerplus SHARED ${MPPLUS_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_multiplayerplus)

set_target_properties(dbp_plugin_multiplayerplus PROPERTIES
    OUTPUT_NAME "DBProMultiplayerPlusDebug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_compile_definitions(dbp_plugin_multiplayerplus PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
    MULTIPLAYERPLUS_EXPORTS
    _MBCS
    _CRT_SECURE_NO_WARNINGS
)

target_include_directories(dbp_plugin_multiplayerplus PUBLIC
    .
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/MultiplayerPlus"
    "${CMAKE_SOURCE_DIR}/ThirdParty/DirectPlay8/include"
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_multiplayerplus PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_multiplayerplus PRIVATE
    ws2_32
    ole32
    user32
)
