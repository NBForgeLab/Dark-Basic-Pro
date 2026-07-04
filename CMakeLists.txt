cmake_minimum_required(VERSION 3.20)

# يجب وضع إعداد المعمارية قبل تعريف المشروع
set(CMAKE_GENERATOR_PLATFORM Win32 CACHE STRING "Force Win32 Platform" FORCE)

project(DarkBasicPro LANGUAGES C CXX)

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
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

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

# Target for testing (TDD)
add_executable(test_logger tests/test_logger.cpp)
target_link_libraries(test_logger PRIVATE spdlog::spdlog)
target_include_directories(test_logger PRIVATE DBProCompiler/DBPCompiler)
