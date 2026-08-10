add_library(dbp_plugin_data STATIC
    CDataC.cpp
)

target_include_directories(dbp_plugin_data PUBLIC
    "."
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${DXSDK_INCLUDE_DIR}"
)

target_compile_definitions(dbp_plugin_data PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
)

if (MSVC)
    target_compile_options(dbp_plugin_data PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h")
endif()
