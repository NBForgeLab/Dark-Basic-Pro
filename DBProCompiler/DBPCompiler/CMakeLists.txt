project(DBPCompiler LANGUAGES C CXX)

# Source files for the static library (excluding Main.cpp and resources)
set(LIB_SOURCES
    ASMWriter.cpp
    DebuggerInterface.cpp
    LeapMarkerManager.cpp
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
    DebuggerInterface.cpp
    CodeGenVisitor.cpp
    SemanticVisitor.cpp
    IRLoweringVisitor.cpp
    TargetCodegen.cpp
    ASTPrinter.cpp
    ASTOptimizer.cpp
    ASTExpressionParser.cpp
    ASTPipelineParser.cpp
    DiagnosticEngine.cpp
    VFSHooks.cpp
    MemoryPE.cpp
    EXEBlock.cpp
    Error.cpp
    FileBuilder.cpp
    IncludeTable.cpp
    InstructionTable.cpp
    InstructionTableEntry.cpp
    LabelTable.cpp
    MathOp.cpp
    Parameter.cpp
    PackageMount.cpp
    ParserResultData.cpp
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
    StatementHelper.cpp
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
set_target_properties(dbp_compiler_lib PROPERTIES UNITY_BUILD ON)
dbp_apply_legacy_cpp_options(dbp_compiler_lib)
target_compile_definitions(dbp_compiler_lib PRIVATE WIN32 _WINDOWS NOGETDXVERSIONCODE UNICODE _UNICODE)
target_link_libraries(dbp_compiler_lib PUBLIC
    dbp_package
    winmm
    dbghelp
    spdlog::spdlog
)
target_include_directories(dbp_compiler_lib PUBLIC
    .
    icons
    ${CMAKE_SOURCE_DIR}/SDK/DB3
    ${CMAKE_SOURCE_DIR}/SDK/BaseClasses
    ${CMAKE_SOURCE_DIR}/SDK/EAX2/Include
    ${CMAKE_SOURCE_DIR}/SDK/NVAPI
    ${DXSDK_INCLUDE_DIR}
)
target_link_directories(dbp_compiler_lib PUBLIC
    ${DXSDK_LIBRARY_DIR}
)

# Generate the resource script from the historical UI resources while binding
# the executable stub to the DarkEXE target produced by this exact build.
# This prevents a stale checked-in runner from being embedded in new programs.
file(
    READ
    "${CMAKE_CURRENT_SOURCE_DIR}/DBPCompiler.rc"
    DBP_COMPILER_RESOURCE_CONTENT
)
string(
    REPLACE
    "\"DarkEXE.exe\""
    "\"$<TARGET_FILE:DarkEXE>\""
    DBP_COMPILER_RESOURCE_CONTENT
    "${DBP_COMPILER_RESOURCE_CONTENT}"
)
string(
    REPLACE
    "\"icon1-db3.ico\""
    "\"${CMAKE_CURRENT_SOURCE_DIR}/icon1-db3.ico\""
    DBP_COMPILER_RESOURCE_CONTENT
    "${DBP_COMPILER_RESOURCE_CONTENT}"
)
string(
    APPEND
    DBP_COMPILER_RESOURCE_CONTENT
    "\n109 X \"$<TARGET_FILE:DarkEXE>\"\n"
)
set(DBP_COMPILER_RESOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/DBPCompiler-$<CONFIG>.rc")
file(
    GENERATE
    OUTPUT "${DBP_COMPILER_RESOURCE}"
    CONTENT "${DBP_COMPILER_RESOURCE_CONTENT}"
)

# Compile main executable linking to the compiler library.
add_executable(
    DBPCompiler
    WIN32
    Main.cpp
    "${DBP_COMPILER_RESOURCE}"
)
add_dependencies(DBPCompiler DarkEXE)
dbp_apply_legacy_cpp_options(DBPCompiler)
target_compile_definitions(DBPCompiler PRIVATE WIN32 _WINDOWS NOGETDXVERSIONCODE UNICODE _UNICODE)
target_link_libraries(DBPCompiler PRIVATE dbp_compiler_lib)

# A compiler build is only usable when its host command database, localized
# diagnostics, debugger, and legacy product plug-ins are deployed beside it.
# Keep the newly built compiler and runner, but publish the immutable support
# surface from the tracked installation instead of silently accepting a
# partially populated output directory.
set(DBP_COMPILER_HOST_ROOT "${CMAKE_SOURCE_DIR}/Install/Compiler")
add_custom_command(
    TARGET DBPCompiler
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:DBPCompiler>/../temp"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DBP_COMPILER_HOST_ROOT}/setup.ini"
        "$<TARGET_FILE_DIR:DBPCompiler>/setup.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DBP_COMPILER_HOST_ROOT}/DBPDebugger.exe"
        "$<TARGET_FILE_DIR:DBPCompiler>/DBPDebugger.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DBP_COMPILER_HOST_ROOT}/BCGCB59.dll"
        "$<TARGET_FILE_DIR:DBPCompiler>/BCGCB59.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DBP_COMPILER_HOST_ROOT}/MD5.dll"
        "$<TARGET_FILE_DIR:DBPCompiler>/MD5.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DBP_COMPILER_HOST_ROOT}/mfc71.dll"
        "$<TARGET_FILE_DIR:DBPCompiler>/mfc71.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DBP_COMPILER_HOST_ROOT}/msvcr71.dll"
        "$<TARGET_FILE_DIR:DBPCompiler>/msvcr71.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DBP_COMPILER_HOST_ROOT}/lang"
        "$<TARGET_FILE_DIR:DBPCompiler>/lang"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DBP_COMPILER_HOST_ROOT}/effects"
        "$<TARGET_FILE_DIR:DBPCompiler>/effects"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DBP_COMPILER_HOST_ROOT}/plugins"
        "$<TARGET_FILE_DIR:DBPCompiler>/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DBP_COMPILER_HOST_ROOT}/plugins-user"
        "$<TARGET_FILE_DIR:DBPCompiler>/plugins-user"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DBP_COMPILER_HOST_ROOT}/plugins-licensed"
        "$<TARGET_FILE_DIR:DBPCompiler>/plugins-licensed"
    VERBATIM
    COMMENT "Deploying the DBPro compiler host runtime"
)
