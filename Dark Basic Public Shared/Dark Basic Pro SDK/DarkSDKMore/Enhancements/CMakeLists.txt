project(DBPPluginEnhancements LANGUAGES C CXX)

set(ENHANCEMENTS_SOURCES
    Core.cpp
    EAX.cpp
    CpuUsage.cpp
    OSCpu.cpp
    OSCpuInfo.cpp
    OSHardDrive.cpp
    OSComputerNames.cpp
    OSDisplay.cpp
    OSDialog.cpp
    OSMemory.cpp
    FileBlocks.cpp
    FileMapping.cpp
    IPC.cpp
    Zlib/adler32.c
    Zlib/aescrypt.c
    Zlib/aeskey.c
    Zlib/aestab.c
    Zlib/compress.c
    Zlib/crc32.c
    Zlib/cZip.cpp
    Zlib/deflate.c
    Zlib/entropy.c
    Zlib/fileenc.c
    Zlib/gzclose.c
    Zlib/gzlib.c
    Zlib/gzread.c
    Zlib/gzwrite.c
    Zlib/hmac.c
    Zlib/infback.c
    Zlib/inffast.c
    Zlib/inflate.c
    Zlib/inftrees.c
    Zlib/ioapi.c
    Zlib/ioapi_buf.c
    Zlib/ioapi_mem.c
    Zlib/iowin32.c
    Zlib/prng.c
    Zlib/pwd2key.c
    Zlib/sha1.c
    Zlib/trees.c
    Zlib/uncompr.c
    Zlib/unzip.c
    Zlib/zip.c
    Zlib/zutil.c
    commands.rc
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
)

add_library(dbp_plugin_enhancements SHARED ${ENHANCEMENTS_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_enhancements)

set_target_properties(dbp_plugin_enhancements PROPERTIES
    OUTPUT_NAME "EnhancementsFREE"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_compile_definitions(dbp_plugin_enhancements PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
    MAIN_EXPORTS
    TARGET_WINDOWS
    _MBCS
    _CRT_SECURE_NO_WARNINGS
)

target_include_directories(dbp_plugin_enhancements PUBLIC
    .
    Zlib
    "${CMAKE_SOURCE_DIR}/SDK/BaseClasses"
    "${CMAKE_SOURCE_DIR}/SDK/DB3"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Include"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error"
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_enhancements PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_enhancements PRIVATE
    winmm
    netapi32
    dxguid
    gdi32
    user32
    advapi32
)
