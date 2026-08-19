project(DBPPluginSystem LANGUAGES C CXX)

set(SYSTEM_SOURCES
    CSystemC.cpp
    dxdiaginfo.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/System/System.rc"
)

add_library(dbp_plugin_system SHARED ${SYSTEM_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_system)

set_target_properties(dbp_plugin_system PROPERTIES
    OUTPUT_NAME "DBProSystemDebug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_include_directories(dbp_plugin_system PUBLIC
    "."
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error"
    "${DXSDK_INCLUDE_DIR}"
)

target_compile_definitions(dbp_plugin_system PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    WIN32
    _WINDOWS
    _MBCS
    _HAS_STD_BYTE=0
)

if (MSVC)
    target_compile_options(dbp_plugin_system PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h")
endif()

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_system PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_system PRIVATE
    dxguid
    wbemuuid
    user32
    gdi32
    advapi32
)
