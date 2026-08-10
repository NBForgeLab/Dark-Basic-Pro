project(DBPPluginInput LANGUAGES C CXX)

set(INPUT_SOURCES
    CInputC.cpp
    Controller.cpp
)

add_library(dbp_plugin_input STATIC ${INPUT_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_input)

target_compile_definitions(dbp_plugin_input PRIVATE
    WIN32
    _WINDOWS
    DIRECTINPUT_VERSION=0x0800
    _MBCS
)

target_include_directories(dbp_plugin_input PUBLIC
    .
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_input PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_input PRIVATE
    dinput8
    dxguid
    winmm
)
