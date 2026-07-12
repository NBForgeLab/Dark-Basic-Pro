cmake_minimum_required(VERSION 3.20)

# يجب وضع إعداد المعمارية قبل تعريف المشروع
set(CMAKE_GENERATOR_PLATFORM Win32 CACHE STRING "Force Win32 Platform" FORCE)

project(DarkBasicPro LANGUAGES C CXX ASM_MASM)

include(CTest)
include(cmake/ProjectOptions.cmake)

# Enable SAFESEH for MASM on Win32
if(MSVC)
    set(CMAKE_ASM_MASM_FLAGS "${CMAKE_ASM_MASM_FLAGS} /safeseh")
endif()

# التحقق من أن المعمارية الحالية هي 32-bit (x86)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "هذا المشروع مخصص حالياً للعمل كـ 32-bit (x86). يرجى تشغيل تهيئة CMake باستخدام خيار المعمارية المناسب: cmake -A Win32 ..")
endif()

# إعداد خيارات لغة C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch and configure spdlog library
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)
dbp_enable_parallel_msvc(spdlog)

# مسارات مخرجات البناء الموحدة
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Resolve the legacy DirectX SDK without assuming this checkout is the primary
# Git worktree. An explicit cache value always wins.
set(DBP_DXSDK_ROOT "" CACHE PATH "DirectX SDK root or Microsoft.DXSDK.D3DX package root")

if(NOT DBP_DXSDK_ROOT AND DEFINED ENV{DXSDK_DIR})
    file(TO_CMAKE_PATH "$ENV{DXSDK_DIR}" DBP_DXSDK_ROOT)
    message(STATUS "Using DirectX SDK from DXSDK_DIR: ${DBP_DXSDK_ROOT}")
endif()

if(NOT DBP_DXSDK_ROOT)
    execute_process(
        COMMAND git rev-parse --path-format=absolute --git-common-dir
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE DBP_GIT_COMMON_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(DBP_GIT_COMMON_DIR)
        get_filename_component(DBP_REPOSITORY_ROOT "${DBP_GIT_COMMON_DIR}" DIRECTORY)
        get_filename_component(DBP_REPOSITORY_PARENT "${DBP_REPOSITORY_ROOT}" DIRECTORY)
        set(DBP_FPSC_DXSDK_CANDIDATE
            "${DBP_REPOSITORY_PARENT}/FPS-Creator-Classic/FPS Creator Editor/dxsdk")
        if(EXISTS "${DBP_FPSC_DXSDK_CANDIDATE}")
            set(DBP_DXSDK_ROOT "${DBP_FPSC_DXSDK_CANDIDATE}")
            message(STATUS "Using DirectX SDK package from sibling FPS Creator Classic checkout")
        endif()
    endif()
endif()

if(EXISTS "${DBP_DXSDK_ROOT}/Include")
    set(DXSDK_INCLUDE_DIR "${DBP_DXSDK_ROOT}/Include")
    set(DXSDK_LIBRARY_DIR "${DBP_DXSDK_ROOT}/Lib/x86")
elseif(EXISTS "${DBP_DXSDK_ROOT}/build/native/include")
    set(DXSDK_INCLUDE_DIR "${DBP_DXSDK_ROOT}/build/native/include")
    set(DXSDK_LIBRARY_DIR "${DBP_DXSDK_ROOT}/build/native/release/lib/x86")
else()
    set(DXSDK_INCLUDE_DIR "")
    set(DXSDK_LIBRARY_DIR "")
    message(WARNING
        "Legacy DirectX SDK not found. Set DBP_DXSDK_ROOT or DXSDK_DIR; "
        "targets that include D3DX headers may not build.")
endif()

# مسارات التضمين العامة (Global Includes)
include_directories(
    ${PROJECT_SOURCE_DIR}/SDK/DB3
    ${PROJECT_SOURCE_DIR}/SDK/BaseClasses
    ${PROJECT_SOURCE_DIR}/SDK/EAX2/Include
    ${PROJECT_SOURCE_DIR}/SDK/NVAPI
    ${DXSDK_INCLUDE_DIR}
)

# مسارات المكتبات العامة
link_directories(
    ${DXSDK_LIBRARY_DIR}
)

# إضافة المشاريع الفرعية للترجمة والمشغل
add_subdirectory(DBProCompiler/DBPCompiler)
add_subdirectory(DBProCompiler/DBPCompilerEXE)

if(BUILD_TESTING)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/release-1.11.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    foreach(gtest_target IN ITEMS gtest gtest_main gmock gmock_main)
        if(TARGET ${gtest_target})
            dbp_enable_parallel_msvc(${gtest_target})
        endif()
    endforeach()
    add_subdirectory(tests)
endif()
