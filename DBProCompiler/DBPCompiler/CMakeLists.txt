cmake_minimum_required(VERSION 3.20)
project(DBPCompiler LANGUAGES C CXX)

# Preprocessor definitions
add_compile_definitions(
    WIN32
    _WINDOWS
    NOGETDXVERSIONCODE
)

if(MSVC)
    add_compile_options(/EHa /W3)
endif()

# Additional include directories
include_directories(
    .
    icons
    ../TGCOnline
)

# Source files for the static library (excluding Main.cpp and resources)
set(LIB_SOURCES
    ASMWriter.cpp
    CompilerContext.cpp
    CompilerArguments.cpp
    CompilationInput.cpp
    CoreRuntimeApi.cpp
    CodeGenerationSession.cpp
    CrashHandler.cpp
    DBMWriter.cpp
    DBPCompiler.cpp
    DataTable.cpp
    DebugInfo.cpp
    Declaration.cpp
    CodeGenVisitor.cpp
    SemanticVisitor.cpp
    DiagnosticEngine.cpp
    VFSHooks.cpp
    MemoryPE.cpp
    EXEBlock.cpp
    Encryptor.cpp
    Error.cpp
    FileBuilder.cpp
    IncludeTable.cpp
    InstructionTable.cpp
    InstructionTableEntry.cpp
    LabelTable.cpp
    MathOp.cpp
    Parameter.cpp
    PeExportInspector.cpp
    ParseFunction.cpp
    ParseInit.cpp
    ParseInstruction.cpp
    ParseJump.cpp
    ParseLoop.cpp
    ParseType.cpp
    ParseUserFunction.cpp
    ProjectManifest.cpp
    RuntimeBundleResolver.cpp
    SourceAssembler.cpp
    Statement.cpp
    StatementChain.cpp
    StatementList.cpp
    Str.cpp
    StructTable.cpp
    Task.cpp
    VarTable.cpp
    icons/DIB.C
    icons/ICONS.C
)

# Force MSVC to compile .C files as C instead of C++ and compile without UNICODE
if(MSVC)
    set_source_files_properties(
        icons/DIB.C
        icons/ICONS.C
        PROPERTIES 
        COMPILE_FLAGS "/TC"
        COMPILE_DEFINITIONS "WIN32;_WINDOWS;NOGETDXVERSIONCODE"
    )
endif()

# Create static library to allow linking with tests
add_library(dbp_compiler_lib STATIC ${LIB_SOURCES})
dbp_apply_legacy_cpp_options(dbp_compiler_lib)
target_compile_definitions(dbp_compiler_lib PRIVATE UNICODE _UNICODE)
target_link_libraries(dbp_compiler_lib PUBLIC winmm dbghelp spdlog::spdlog)
target_include_directories(dbp_compiler_lib PUBLIC . icons ../TGCOnline)

# Compile main executable linking to the compiler library
add_executable(DBPCompiler WIN32 Main.cpp DBPCompiler.rc)
dbp_apply_legacy_cpp_options(DBPCompiler)
target_compile_definitions(DBPCompiler PRIVATE UNICODE _UNICODE)
target_link_libraries(DBPCompiler PRIVATE dbp_compiler_lib)
