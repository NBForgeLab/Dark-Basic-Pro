cmake_minimum_required(VERSION 3.20)

# يجب وضع إعداد المعمارية قبل تعريف المشروع
set(CMAKE_GENERATOR_PLATFORM Win32 CACHE STRING "Force Win32 Platform" FORCE)

project(DarkBasicPro LANGUAGES C CXX ASM_MASM)

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

if(MSVC)
    add_compile_options(/EHsc /W3 /Zc:forScope-)
endif()

# Fetch and configure spdlog library
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# مسارات مخرجات البناء الموحدة
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# التحقق من وجود مسار DirectX SDK (August 2007)
if(DEFINED ENV{DXSDK_DIR})
    set(DXSDK_INCLUDE_DIR "$ENV{DXSDK_DIR}/Include")
    set(DXSDK_LIBRARY_DIR "$ENV{DXSDK_DIR}/Lib/x86")
    message(STATUS "Found DirectX SDK (August 2007) via environment: $ENV{DXSDK_DIR}")
elseif(EXISTS "${PROJECT_SOURCE_DIR}/../FPS-Creator-Classic/FPS Creator Editor/dxsdk")
    set(DXSDK_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/../FPS-Creator-Classic/FPS Creator Editor/dxsdk/build/native/include")
    set(DXSDK_LIBRARY_DIR "${PROJECT_SOURCE_DIR}/../FPS-Creator-Classic/FPS Creator Editor/dxsdk/build/native/release/lib/x86")
    message(STATUS "Found DirectX SDK (August 2007) via sibling FPS-Creator-Classic dxsdk package.")
else()
    set(DXSDK_INCLUDE_DIR "")
    set(DXSDK_LIBRARY_DIR "")
    message(WARNING "DirectX SDK not found. Windows SDK defaults will be used.")
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

# Fetch Google Test
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/release-1.11.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Target for testing (Google Test suite)
add_executable(dbp_tests
    tests/test_main.cpp
    tests/test_logger.cpp
    tests/test_vartable.cpp
    tests/test_context.cpp
    tests/test_str.cpp
    tests/test_structural.cpp
    tests/test_unicode.cpp
    tests/test_raw_input.cpp
    tests/test_ast.cpp
    tests/test_diagnostics.cpp
    "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/CInputC.cpp"
    "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/Controller.cpp"
    "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System/dynamic_call.asm"
)
target_link_libraries(dbp_tests PRIVATE
    gtest
    dbp_compiler_lib
    spdlog::spdlog
    xinput
)
target_link_options(dbp_tests PRIVATE /SAFESEH:NO)
target_compile_definitions(dbp_tests PRIVATE DBP_TESTS_COMPILATION)
target_include_directories(dbp_tests PRIVATE 
    DBProCompiler/DBPCompiler
    "Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input"
)
