cmake_minimum_required(VERSION 4.2)

# DarkBasic Pro is a pure native x64 codebase: 64-bit only, no 32-bit
# (Win32/x86) build platform exists anymore.
set(CMAKE_GENERATOR_PLATFORM x64 CACHE STRING "Target Platform (x64 only)" FORCE)

project(DarkBasicPro VERSION 1.0.5 LANGUAGES C CXX ASM_MASM)

include(CTest)
include(cmake/ProjectOptions.cmake)

# clang-tidy static analysis integration (opt-in).
# When enabled, CMake will run clang-tidy on every translation unit during build.
option(DBP_ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
if(DBP_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy not found, static analysis disabled")
    endif()
endif()

# Enforce the native x64-only policy: refuse any 32-bit configuration.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "DarkBasic Pro supports native x64 only: 32-bit (Win32/x86) "
        "configurations are no longer supported.")
endif()
message(STATUS "DarkBasic Pro targeting 64-bit architecture (x64).")

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

set(DXSDK_ARCH_DIR "x64")

if(EXISTS "${DBP_DXSDK_ROOT}/Include")
    set(DXSDK_INCLUDE_DIR "${DBP_DXSDK_ROOT}/Include")
    set(DXSDK_LIBRARY_DIR "${DBP_DXSDK_ROOT}/Lib/${DXSDK_ARCH_DIR}")
elseif(EXISTS "${DBP_DXSDK_ROOT}/build/native/include")
    set(DXSDK_INCLUDE_DIR "${DBP_DXSDK_ROOT}/build/native/include")
    set(DXSDK_LIBRARY_DIR "${DBP_DXSDK_ROOT}/build/native/release/lib/${DXSDK_ARCH_DIR}")
else()
    set(DXSDK_INCLUDE_DIR "")
    set(DXSDK_LIBRARY_DIR "")
    message(WARNING
        "Legacy DirectX SDK not found. Set DBP_DXSDK_ROOT or DXSDK_DIR; "
        "targets that include D3DX headers may not build.")
endif()

# Target-specific include and library paths are configured per-target in their
# own CMakeLists.txt files (see dbp_compiler_lib in DBProCompiler/DBPCompiler).

# إضافة المشاريع الفرعية للترجمة والمشغل والمصحح والمكتبات الفرعية
add_subdirectory(DBProShared/Package)
include(cmake/DBPPlugins.cmake)
add_subdirectory(DBProCompiler/DBPCompiler)
add_subdirectory(DBProCompiler/DBPCompilerEXE)
add_subdirectory(DBProCompiler/DBPDebugger)

# fuzz/ hosts two kinds of targets with different reachability requirements:
#   * the corpus seed generator — a plain host tool that must exist on every
#     toolchain (the MSVC corpus workflow and the Clang fuzzing workflow both
#     run the same binary), so it is built whenever tests OR fuzzers are on;
#   * the libFuzzer targets — Clang-only, gated inside fuzz/CMakeLists.txt.
# The directory is entered once, so the seed generator is defined exactly once
# (defining it here and again under tests/ is a hard CMake error: duplicate target).
if(DBP_BUILD_FUZZERS OR BUILD_TESTING)
    add_subdirectory(fuzz)
endif()

# Install and CPack configuration
set(DBP_CPACK_CONFIGURED 1 CACHE INTERNAL "")
set(DBP_INSTALL_RULES_DEFINED 1 CACHE INTERNAL "")

# Install executables
install(TARGETS DBPCompiler DarkEXE DBPDebugger
        RUNTIME DESTINATION bin
        COMPONENT Runtime)

# Install data files from Install/ directory
install(DIRECTORY Install/Help DESTINATION . COMPONENT Data)
install(DIRECTORY Install/Editor DESTINATION . COMPONENT Data)
install(DIRECTORY Install/Projects DESTINATION . COMPONENT Data)
install(FILES Install/Launch.CFG Install/Launch.exe DESTINATION . COMPONENT Data)

# Install compiler support files
install(FILES Install/Compiler/setup.ini DESTINATION bin COMPONENT Runtime)
install(DIRECTORY Install/Compiler/lang DESTINATION bin COMPONENT Runtime)
install(DIRECTORY Install/Compiler/effects DESTINATION bin COMPONENT Runtime)
install(DIRECTORY Install/Compiler/plugins DESTINATION bin COMPONENT Runtime)
install(DIRECTORY Install/Compiler/plugins-user DESTINATION bin COMPONENT Runtime)
install(DIRECTORY Install/Compiler/plugins-licensed DESTINATION bin COMPONENT Runtime)

# Install LICENSE
install(FILES LICENSE DESTINATION . COMPONENT Documentation)

# CPack configuration
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "The Game Creators")
set(CPACK_PACKAGE_DESCRIPTION "Dark Basic Pro - BASIC game development environment")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# Generator configuration (ZIP always available; NSIS if present)
set(CPACK_GENERATOR "ZIP")
find_program(NSIS_MAKE makensis)
if(NSIS_MAKE)
    list(APPEND CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Dark Basic Pro ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "DarkBasicPro")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
endif()

include(CPack)

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
