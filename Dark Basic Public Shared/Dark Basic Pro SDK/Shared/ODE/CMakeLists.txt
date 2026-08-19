project(DBPPluginODE LANGUAGES C CXX)

set(ODE_SOURCES
    ODEPhysics.CPP
    sODEBone.cpp
    sODEJoint.cpp
    sODERagdoll.cpp
    ODE.rc
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Data/CDataC.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
)

add_library(dbp_plugin_ode SHARED ${ODE_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_ode)

set_target_properties(dbp_plugin_ode PROPERTIES
    OUTPUT_NAME "DBProODEDebug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_compile_definitions(dbp_plugin_ode PRIVATE
    WIN32
    _WINDOWS
    _MBCS
    _CRT_SECURE_NO_WARNINGS
    dSINGLE
)

target_include_directories(dbp_plugin_ode PUBLIC
    .
    "${CMAKE_SOURCE_DIR}/SDK/DB3"
    "${CMAKE_SOURCE_DIR}/SDK/ODE/ode/include"
    "${CMAKE_SOURCE_DIR}/SDK/ODE/ode/build_x64_single/include"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Data"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Objects"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat"
    ${DXSDK_INCLUDE_DIR}
)

target_link_directories(dbp_plugin_ode PRIVATE
    "${CMAKE_SOURCE_DIR}/SDK/ODE/ode/build_x64_single/Release"
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_ode PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_ode PRIVATE
    ode_singles
    winmm
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
