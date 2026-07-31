cmake_minimum_required(VERSION 3.25)

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

# Dependency revisions are immutable commits corresponding to the upstream
# v1.17.0 releases (Apache-2.0 for spdlog, BSD-3-Clause for GoogleTest).
include(FetchContent)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG 79524ddd08a4ec981b7fea76afd08ee05f83755d
    GIT_SHALLOW FALSE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(spdlog)
dbp_enable_parallel_msvc(spdlog)
dbp_enable_sanitizers(spdlog)

# Zstandard v1.5.7 fixes a rare long-session corruption issue in 32-bit mode.
# Pin the signed upstream release commit and build only the static library.
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_DICTBUILDER OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_DEPRECATED OFF CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG f8745da6ff1ad1e7bab384bd1f9d742439278e99
    GIT_SHALLOW FALSE
    GIT_PROGRESS TRUE
    SOURCE_SUBDIR build/cmake
)
FetchContent_MakeAvailable(zstd)
if(TARGET libzstd_static)
    dbp_enable_parallel_msvc(libzstd_static)
    dbp_enable_sanitizers(libzstd_static)
endif()

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

# Target-specific include and library paths are configured per-target in their
# own CMakeLists.txt files (see dbp_compiler_lib in DBProCompiler/DBPCompiler).

# إضافة المشاريع الفرعية للترجمة والمشغل والمصحح
add_subdirectory(DBProShared/Package)
add_subdirectory(DBProCompiler/DBPCompiler)
add_subdirectory(DBProCompiler/DBPCompilerEXE)
add_subdirectory(DBProCompiler/DBPDebugger)

if(DBP_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()

if(BUILD_TESTING)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723
        GIT_SHALLOW FALSE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(googletest)
    foreach(gtest_target IN ITEMS gtest gtest_main gmock gmock_main)
        if(TARGET ${gtest_target})
            dbp_enable_parallel_msvc(${gtest_target})
            dbp_enable_sanitizers(${gtest_target})
        endif()
    endforeach()
    add_subdirectory(tests)
endif()
