project(DarkEXE LANGUAGES C CXX)

# Source files (using CMAKE_SOURCE_DIR to correctly resolve root paths)
set(SOURCES
    DarkEXE.cpp
    RuntimePackageBootstrap.cpp
    getdxver.cpp
    ${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler/CoreRuntimeApi.cpp
    ${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler/EXEBlock.cpp
    ${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler/VFSHooks.cpp
    ${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler/PackageMount.cpp
    ${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler/MemoryPE.cpp
    ${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler/CrashHandler.cpp
    dbproexe.rc
)

# Compile as a Windows GUI subsystem application (no console window)
add_executable(DarkEXE WIN32 ${SOURCES})
dbp_apply_legacy_cpp_options(DarkEXE)
target_compile_definitions(DarkEXE PRIVATE _WINDOWS DARKEXE UNICODE _UNICODE)
target_include_directories(DarkEXE PRIVATE
    ${CMAKE_SOURCE_DIR}/DBProCompiler/DBPCompiler
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/MemoryManager"
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/EAX2/Include
    ${CMAKE_SOURCE_DIR}/SDK/NVAPI
    ${DXSDK_INCLUDE_DIR}
)
target_link_directories(DarkEXE PRIVATE
    ${DXSDK_LIBRARY_DIR}
)

# Target libraries
target_link_libraries(DarkEXE PRIVATE
    dbp_package
    winmm
    Version
    dxguid
    odbc32
    odbccp32
    dbghelp
    spdlog::spdlog
)
