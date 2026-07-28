add_executable(dbp_tests
    test_main.cpp
    test_logger.cpp
    test_vartable.cpp
    test_context.cpp
    test_str.cpp
    test_structural.cpp
    test_unicode.cpp
    test_raw_input.cpp
    test_ast.cpp
    test_diagnostics.cpp
    test_vfs.cpp
    test_cli.cpp
    test_concurrency.cpp
    test_project_manifest.cpp
    test_source_assembler.cpp
    test_compilation_input.cpp
    test_codegen_session.cpp
    test_runtime_bundle.cpp
    test_pe_export_inspector.cpp
    test_core_runtime_api.cpp
    test_x64_pointer_safety.cpp
    test_modern_cpp_safety.cpp
    test_exeblock_memory.cpp
    test_exeblock_arrays.cpp
    test_declaration.cpp
    test_filebuilder.cpp
    test_statement_label.cpp
    test_statement_loop.cpp
    test_statement_jump.cpp
        test_statement_declaration.cpp
        test_statement_allocation.cpp
        test_statement_instruction.cpp
        test_statement_type.cpp
        test_statement_deallocation.cpp
        test_statement_userfunction.cpp
        test_statement_assignment.cpp
        test_statement_token.cpp
        test_statement_data.cpp
        test_statement_paramlist.cpp
        test_statement_expression.cpp
    test_asmwriter_emission.cpp
    test_mathop_expression.cpp
    test_instruction_table.cpp
    test_statement_string_helpers.cpp
    test_debugger.cpp
    test_statement_object.cpp
    test_package_byte_codec.cpp
    test_package_path.cpp
    test_package_crypto.cpp
    test_package_format.cpp
    test_package_compression.cpp
    test_package_keys.cpp
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/CInputC.cpp"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/Controller.cpp"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System/dynamic_call.asm"
)

set_source_files_properties(
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System/dynamic_call.asm"
    PROPERTIES LANGUAGE ASM_MASM
)

dbp_apply_modern_cpp_options(dbp_tests)

target_link_libraries(dbp_tests PRIVATE
    dbp_package
    dbp_compiler_lib
    GTest::gtest
    spdlog::spdlog
    xinput
)

target_compile_definitions(dbp_tests PRIVATE DBP_TESTS_COMPILATION)

target_include_directories(dbp_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPCompiler"
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPDebugger"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input"
)

if(MSVC)
    target_link_options(dbp_tests PRIVATE /SAFESEH:NO)
endif()

include(GoogleTest)

set(dbp_test_properties TIMEOUT 60)
if(DBP_ENABLE_ASAN AND MSVC)
    get_filename_component(dbp_msvc_runtime_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    list(APPEND dbp_test_properties
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${dbp_msvc_runtime_directory}"
    )
endif()

gtest_discover_tests(dbp_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES ${dbp_test_properties}
)
