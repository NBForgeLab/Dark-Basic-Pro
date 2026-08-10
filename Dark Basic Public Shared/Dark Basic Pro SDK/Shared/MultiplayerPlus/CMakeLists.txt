add_library(dbp_plugin_multiplayerplus STATIC
    Multiplayer.cpp
    netvoice.cpp
)

target_include_directories(dbp_plugin_multiplayerplus PUBLIC
    "."
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${DXSDK_INCLUDE_DIR}"
)

target_compile_definitions(dbp_plugin_multiplayerplus PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(dbp_plugin_multiplayerplus PRIVATE
    ws2_32.lib
    dxguid.lib
)

if (MSVC)
    target_compile_options(dbp_plugin_multiplayerplus PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h")
endif()
