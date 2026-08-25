project(DBPPluginCore LANGUAGES C CXX)

set(CORE_SOURCES
    DBDLLCore.cpp
    DBDLLCoreInternal.cpp
    DBDLLDisplay.cpp
    DBDLLEXT.cpp
    DBDLLExtCalls.cpp
    DBDLLGDI.cpp
    RenderList.cpp
    EncryptedFile.cpp
    globstruct.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/DBPCompiler/Encryptor.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/Core/DBDLLCore.rc"
)

add_library(dbp_plugin_core STATIC ${CORE_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_core)

target_compile_definitions(dbp_plugin_core PRIVATE
    WIN32
    _WINDOWS
    DIRECTINPUT_VERSION=0x0800
    _MBCS
)

# The DarkBASIC runtime core DLL. Built from the same modernized sources as
# the static library so the exported ABI (64-bit pointer returns, current
# GlobStruct layout) matches what the 64-bit compiler and its generated
# executables expect. DARKSDK_COMPILE must stay undefined so DARKSDK
# expands to __declspec(dllexport) and the runtime surface is exported.
add_library(DBProCore SHARED ${CORE_SOURCES})
dbp_apply_legacy_cpp_options(DBProCore)

set_target_properties(DBProCore PROPERTIES
    OUTPUT_NAME "DBProCore"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_compile_definitions(DBProCore PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
    DIRECTINPUT_VERSION=0x0800
    _MBCS
)

target_include_directories(DBProCore PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${DXSDK_INCLUDE_DIR}
)

target_include_directories(dbp_plugin_core PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_core PRIVATE ${DXSDK_LIBRARY_DIR})
    target_link_directories(DBProCore PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_core PRIVATE
    dinput8
    dxguid
    winmm
    user32
    gdi32
    advapi32
)

target_link_libraries(DBProCore PRIVATE
    dinput8
    dxguid
    winmm
    user32
    gdi32
    advapi32
    odbc32
    odbccp32
)
