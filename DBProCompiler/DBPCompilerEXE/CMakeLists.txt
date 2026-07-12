cmake_minimum_required(VERSION 3.20)
project(DarkEXE LANGUAGES C CXX)

# Preprocessor definitions
add_compile_definitions(
    WIN32
    _WINDOWS
    DARKEXE
    UNICODE
    _UNICODE
)

if(MSVC)
    add_compile_options(/EHsc /W3)
endif()

# Additional include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/MemoryManager"
)

# Source files (using CMAKE_SOURCE_DIR to correctly resolve root paths)
set(SOURCES
    DarkEXE.cpp
    FileReader.cpp
    getdxver.cpp
    ${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler/EXEBlock.cpp
    ${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler/VFSHooks.cpp
    ${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler/MemoryPE.cpp
    dbproexe.rc
)

# Compile as a Win32 GUI Application
add_executable(DarkEXE WIN32 ${SOURCES})

# Target libraries
target_link_libraries(DarkEXE PRIVATE
    winmm
    Version
    dxguid
    odbc32
    odbccp32
    spdlog::spdlog
)
