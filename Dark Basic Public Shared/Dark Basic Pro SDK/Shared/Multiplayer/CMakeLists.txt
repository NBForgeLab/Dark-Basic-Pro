add_library(dbp_plugin_multiplayer STATIC
    CMultiplayerC.cpp
    Network/CNetwork.cpp
    Network/NetQueue.cpp
)

target_include_directories(dbp_plugin_multiplayer PUBLIC
    "."
    Network
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${DXSDK_INCLUDE_DIR}"
)

target_compile_definitions(dbp_plugin_multiplayer PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(dbp_plugin_multiplayer PRIVATE
    ws2_32.lib
    dxguid.lib
)

if (MSVC)
    target_compile_options(dbp_plugin_multiplayer PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h")
endif()
