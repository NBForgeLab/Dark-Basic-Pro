# Phase 1: Unified CMake Build System (Status: Completed ✅)

## 🎯 Goal
Replace over 40 outdated Visual Studio project files (`.vcproj` and `.vcxproj`) with a single, centralized **CMake** build configuration. This ensures easy configuration of modules and dependencies, enabling the entire project to compile with a single build command instead of manual IDE project upgrades.

---

## 🏗️ Root CMake Architecture (Implemented)

The root `CMakeLists.txt` file is located in the repository root. It initializes global configurations, configures standards, and sets up path fallbacks for the environment.

### Implemented Root `CMakeLists.txt`
```cmake
cmake_minimum_required(VERSION 4.2)

# Must be set before the project() command; x64 is the default target
# platform, Win32 remains available via the windows-x86-* presets.
if(NOT CMAKE_GENERATOR_PLATFORM)
    set(CMAKE_GENERATOR_PLATFORM x64 CACHE STRING "Target Platform (x64 or Win32)" FORCE)
endif()

project(DarkBasicPro LANGUAGES C CXX)

# C++ Standard configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # Enable standard C++ exception handling, warnings, and force legacy for-scope behavior
    add_compile_options(/EHsc /W3 /Zc:forScope-)
endif()

# Unified output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# Detect DirectX SDK (August 2007) with automated fallback to sibling NuGet package
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

# Global include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/SDK/DB3
    ${PROJECT_SOURCE_DIR}/SDK/BaseClasses
    ${PROJECT_SOURCE_DIR}/SDK/EAX2/Include
    ${PROJECT_SOURCE_DIR}/SDK/NVAPI
    ${DXSDK_INCLUDE_DIR}
)

# Global library search directories
link_directories(
    ${DXSDK_LIBRARY_DIR}
)

# Add compiler and runner subdirectories
add_subdirectory(DBProCompiler/DBPCompiler)
add_subdirectory(DBProCompiler/DBPCompilerEXE)
```

---

## 🔧 Sub-Component Implementations

### 1. DBPCompiler Subdirectory (`DBProCompiler/DBPCompiler/CMakeLists.txt`)
* Compiles the core compiler `DBPCompiler.exe`.
* Includes `icons/DIB.C` and `icons/ICONS.C` for file building support.
* **C Compilation Rule Forcing**: Standard MSVC treats files with capital `.C` extensions as C++ source files. Since these legacy files use C-style implicit pointer conversions (e.g. `void*` to struct pointers), we forced C compilation using `/TC` compiler flags:
  ```cmake
  if(MSVC)
      set_source_files_properties(
          icons/DIB.C
          icons/ICONS.C
          PROPERTIES COMPILE_FLAGS "/TC"
      )
  endif()
  ```
* Compiled as `WIN32` GUI Application to resolve entry point mismatch and avoid `_main` linking errors.

### 2. DBPCompilerEXE Subdirectory (`DBProCompiler/DBPCompilerEXE/CMakeLists.txt`)
* Compiles the runner `DarkEXE.exe`.
* References `${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler/EXEBlock.cpp` directly to resolve all runtime initialization and JIT packaging code.
* Compiled as `WIN32` GUI Application to resolve entry point mismatch and avoid `_main` linking errors.
* Linked to necessary system libraries: `winmm`, `Version`, `dxguid`, `odbc32`, and `odbccp32`.

---

## 🔄 Verification & Build Steps
We verified the builds using a clean, reproducible setup:
```bash
Remove-Item -Path build -Recurse -Force -ErrorAction SilentlyContinue
mkdir build
cd build
cmake -A Win32 ..
cmake --build . --config Release
```

### Generated Binaries (Unified Output Path: `bin/Release`):
* **`DBPCompiler.exe`** (Successfully compiled, linked, and ready).
* **`DarkEXE.exe`** (Successfully compiled, linked, and ready).

---

## 🚀 Benefits Achieved
* **One-Click Build**: Builds the entire compile-and-run environment with a single CMake instruction.
* **Automatic Dependency Fallback**: Resolves DirectX SDK requirements without manual installation on new developer systems by utilizing sibling NuGet packages.
* **Legacy Conformance Handling**: Integrates clean MSVC settings for scope rules (`/Zc:forScope-`) and C source compilers (`/TC`), bypassing code modifications on legacy source files.
