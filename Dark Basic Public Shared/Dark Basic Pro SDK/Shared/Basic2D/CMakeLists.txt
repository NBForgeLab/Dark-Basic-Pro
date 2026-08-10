add_library(dbp_plugin_basic2d STATIC
    CBasic2DC.cpp
)

target_include_directories(dbp_plugin_basic2d PUBLIC
    "."
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Image"
    "${DXSDK_INCLUDE_DIR}"
)

target_compile_definitions(dbp_plugin_basic2d PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
)

if (MSVC)
    target_compile_options(dbp_plugin_basic2d PRIVATE /FI"${CMAKE_SOURCE_DIR}/cmake/dbp_bsp_types.h")
endif()
