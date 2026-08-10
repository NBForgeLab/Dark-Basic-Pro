project(DBPPluginCore LANGUAGES C CXX)

set(CORE_SOURCES
    DBDLLCore.cpp
    DBDLLCoreInternal.cpp
    DBDLLDisplay.cpp
    DBDLLEXT.cpp
    DBDLLExtCalls.cpp
    DBDLLGDI.cpp
    RenderList.cpp
    SteamCheckForWorkshop.cpp
    globstruct.cpp
)

add_library(dbp_plugin_core STATIC ${CORE_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_core)

target_compile_definitions(dbp_plugin_core PRIVATE
    WIN32
    _WINDOWS
    DIRECTINPUT_VERSION=0x0800
    _MBCS
)

target_include_directories(dbp_plugin_core PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_core PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_core PRIVATE
    dinput8
    dxguid
    winmm
    user32
    gdi32
    advapi32
)
