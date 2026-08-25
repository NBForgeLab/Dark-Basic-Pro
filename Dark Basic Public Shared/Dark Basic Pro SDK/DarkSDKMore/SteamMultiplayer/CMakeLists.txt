# SteamMultiplayer (x64 stub)
#
# This CMake target builds a minimal SteamMultiplayer.dll that satisfies the
# runtime contract the Dark Basic Pro Core expects, without the Valve Steamworks
# SDK. The real implementation lives in the legacy .vcxproj in this folder and
# cannot be built here (no steam_api.h / steam_api.lib). See steam_stub.cpp.
#
# It is intentionally a SHARED library whose OUTPUT_NAME matches the DLL the
# Core loads by name ("SteamMultiplayer.dll").

project(DBPSteamMultiplayerStub LANGUAGES C CXX)

set(STEAM_MULTIPLAYER_STUB_SOURCES
    steam_stub.cpp
    steam_stub.def
)

add_library(dbp_plugin_steammultiplayer SHARED ${STEAM_MULTIPLAYER_STUB_SOURCES})

set_target_properties(dbp_plugin_steammultiplayer PROPERTIES
    OUTPUT_NAME "SteamMultiplayer"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_compile_features(dbp_plugin_steammultiplayer PRIVATE cxx_std_20)

target_compile_definitions(dbp_plugin_steammultiplayer PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
)

# The Core calls LoadLibrary("SteamMultiplayer.dll") by plain name, which
# searches the host executable's directory first. Copy the stub next to the
# compiler EXE so it is discovered at runtime.
add_custom_command(TARGET dbp_plugin_steammultiplayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:dbp_plugin_steammultiplayer>"
        "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/$<TARGET_FILE_NAME:dbp_plugin_steammultiplayer>"
    COMMENT "Copying SteamMultiplayer.dll stub next to the executable"
)
