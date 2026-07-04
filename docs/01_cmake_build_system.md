# Phase 1: Unified CMake Build System

## 🎯 Goal
Replace over 40 outdated Visual Studio project files (`.vcproj` and `.vcxproj`) with a single, centralized **CMake** build configuration. This ensures easy configuration of modules and dependencies, enabling the entire project to compile with a single build command instead of manual IDE project upgrades.

---

## 🏗️ Proposed Root CMake Structure

A root `CMakeLists.txt` file will be created in the repository root to initialize the build configurations and define global compiler flags.

### Root `CMakeLists.txt` Template
```cmake
cmake_minimum_required(VERSION 3.20)

# Must be set before the project() command to enforce 32-bit (x86)
set(CMAKE_GENERATOR_PLATFORM Win32 CACHE STRING "Force Win32 Platform" FORCE)

project(DarkBasicPro LANGUAGES C CXX)

# Verify 32-bit (x86) target
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This project is currently 32-bit (x86) only. Please configure CMake with -A Win32.")
endif()

# C++ Standard configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Unified output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# Detect DirectX SDK (August 2007)
if(DEFINED ENV{DXSDK_DIR})
    set(DXSDK_INCLUDE_DIR "$ENV{DXSDK_DIR}/Include")
    set(DXSDK_LIBRARY_DIR "$ENV{DXSDK_DIR}/Lib/x86")
    message(STATUS "Found DirectX SDK (August 2007): $ENV{DXSDK_DIR}")
else()
    set(DXSDK_INCLUDE_DIR "")
    set(DXSDK_LIBRARY_DIR "")
    message(WARNING "DXSDK_DIR environment variable not found. Using standard Windows SDK headers.")
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

# Add subdirectories
add_subdirectory(DBProCompiler/DBPCompiler)
add_subdirectory(DBProCompiler/DBPCompilerEXE)
```

---

## 🔄 Implementation Steps

1. **Set Up Project Subdirectories**:
   * Add a `CMakeLists.txt` file in [DBPCompiler](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler) defining the DBPCompiler executable and its source list.
   * Add a `CMakeLists.txt` file in [DBPCompilerEXE](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompilerEXE) to build the runner executable `DarkEXE.exe`.
   * Follow the same structure for SDK plugins (Graphics, Sound, Input, etc.).
2. **Handle Legacy Compiler Conformance**:
   * Since this legacy C++ code relies on variables declared in `for` loops persisting in the outer scope, we must add MSVC-specific compilation flags `/Zc:forScope-` to maintain compatibility without modifying thousands of loops.
3. **Build & Verify**:
   * Run the configuration and build commands:
     ```bash
     mkdir build
     cd build
     cmake -A Win32 ..
     cmake --build . --config Release
     ```
   * Confirm that executables are outputted to the unified `bin/` directory.

---

## 🚀 Benefits
* **One-Click Build**: Build the entire DarkBasic Pro environment with a single command.
* **Flexible Dependency Management**: Simple inclusion of third-party libraries (e.g., breaking DirectX SDK dependencies via NuGet packages).
* **Modern Development Flow**: Developers can compile, run, and edit code with modern editors (like VS Code, CLion, or modern Visual Studio) without relying on obsolete IDE project formats.
