cmake_minimum_required(VERSION 3.20)
project(DBPCompiler LANGUAGES C CXX)

# Preprocessor definitions
add_compile_definitions(
    WIN32
    _WINDOWS
    NOGETDXVERSIONCODE
)

if(MSVC)
    add_compile_options(/EHsc /W3)
endif()

# Additional include directories
include_directories(
    .
    icons
    ../TGCOnline
)

# Source files
set(SOURCES
    ASMWriter.cpp
    DBMWriter.cpp
    DBPCompiler.cpp
    DataTable.cpp
    DebugInfo.cpp
    Declaration.cpp
    EXEBlock.cpp
    Encryptor.cpp
    Error.cpp
    FileBuilder.cpp
    IncludeTable.cpp
    InstructionTable.cpp
    InstructionTableEntry.cpp
    LabelTable.cpp
    Main.cpp
    MathOp.cpp
    Parameter.cpp
    ParseFunction.cpp
    ParseInit.cpp
    ParseInstruction.cpp
    ParseJump.cpp
    ParseLoop.cpp
    ParseType.cpp
    ParseUserFunction.cpp
    Statement.cpp
    StatementChain.cpp
    StatementList.cpp
    Str.cpp
    StructTable.cpp
    Task.cpp
    VarTable.cpp
    icons/DIB.C
    icons/ICONS.C
    ../TGCOnline/CertificateKey.cpp
    DBPCompiler.rc
)

# Force MSVC to compile .C files as C instead of C++ (resolves void* conversion errors)
if(MSVC)
    set_source_files_properties(
        icons/DIB.C
        icons/ICONS.C
        PROPERTIES COMPILE_FLAGS "/TC"
    )
endif()

# Compile as a Win32 GUI Application (resolves missing _main error)
add_executable(DBPCompiler WIN32 ${SOURCES})

# Target libraries
target_link_libraries(DBPCompiler PRIVATE winmm spdlog::spdlog)
