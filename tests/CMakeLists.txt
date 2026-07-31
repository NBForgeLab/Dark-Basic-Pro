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
    test_exeblock_wx.cpp
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
    test_leap_marker_manager.cpp
    test_debugger_interface.cpp
    test_machine_code_buffer.cpp
    test_reference_tracker.cpp
    test_tokenizer.cpp
    test_statement_tokenizer_delegation.cpp
    test_task_emitter.cpp
    test_task_emitter_passes.cpp
    test_pe_builder.cpp
    test_pe_builder_headers.cpp
    test_tokenizer_keywords.cpp
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
    test_package_writer.cpp
    test_package_reader.cpp
    test_legacy_pck_reader.cpp
    test_package_runtime_metadata.cpp
    test_package_mount.cpp
    test_runtime_package_startup.cpp
    test_text_line_splitter.cpp
    test_safe_dll_loading.cpp
    test_error.cpp
    test_unsafe_string_patterns.cpp
    test_token_enum_values.cpp
    test_instruction_table_enum_values.cpp
    test_asmwriter_enum_values.cpp
    test_clang_tidy_integration.cpp
    test_coverage_integration.cpp
    test_install_rules.cpp
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPCompilerEXE/RuntimePackageBootstrap.cpp"
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

target_compile_definitions(dbp_tests PRIVATE
    DBP_TESTS_COMPILATION
    DBP_TEST_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
)

if(DBP_ENABLE_COVERAGE)
    target_compile_definitions(dbp_tests PRIVATE DBP_COVERAGE_ENABLED)
endif()

target_include_directories(dbp_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPCompiler"
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPDebugger"
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPCompilerEXE"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_CURRENT_BINARY_DIR}"  # generated InstallConfig.h
)

# Generate InstallConfig.h from template so tests can verify CMake config
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/InstallConfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/InstallConfig.h"
    @ONLY
)

if(MSVC)
    target_link_options(dbp_tests PRIVATE /SAFESEH:NO)
endif()

set(dbp_test_properties TIMEOUT 180)
if(DBP_ENABLE_ASAN AND MSVC)
    get_filename_component(dbp_msvc_runtime_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    list(APPEND dbp_test_properties
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${dbp_msvc_runtime_directory}"
    )
endif()
if(DBP_ENABLE_UBSAN AND MSVC)
    get_filename_component(dbp_msvc_runtime_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    list(APPEND dbp_test_properties
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${dbp_msvc_runtime_directory}"
    )
endif()

# Running the suite as one CTest process avoids CMake's finite capture limit for
# large `--gtest_list_tests` output. It also exercises process-global compiler
# state across tests instead of accidentally hiding lifecycle bugs behind one
# process per case.
# ---------------------------------------------------------------------------
# UBSan probe tests — separate executable so that a UBSan abort does not
# take down the main dbp_tests suite.  Only built when UBSan is enabled.
# ---------------------------------------------------------------------------
if(DBP_ENABLE_UBSAN AND MSVC_VERSION GREATER_EQUAL 1936)
    add_executable(dbp_ubsan_probe_tests
        test_ubsan_detection.cpp
    )
    target_compile_features(dbp_ubsan_probe_tests PRIVATE cxx_std_17)
    target_link_libraries(dbp_ubsan_probe_tests PRIVATE GTest::gtest)

    # MSVC currently ignores /fsanitize=undefined (D9002).  The probe tests
    # detect UBSan absence at runtime and GTEST_SKIP() gracefully.
    # When MSVC adds real UBSan support the flag will activate automatically.
    if(NOT MSVC)
        target_compile_options(dbp_ubsan_probe_tests PRIVATE
            -fsanitize=undefined -fno-omit-frame-pointer
        )
        target_link_options(dbp_ubsan_probe_tests PRIVATE
            -fsanitize=undefined
        )
    endif()

    add_test(NAME dbp_ubsan_probe_tests COMMAND dbp_ubsan_probe_tests)
    set(dbp_ubsan_probe_properties TIMEOUT 60)
    if(MSVC)
        get_filename_component(dbp_msvc_rt_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        list(APPEND dbp_ubsan_probe_properties
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${dbp_msvc_rt_dir}"
        )
    endif()
    set_tests_properties(dbp_ubsan_probe_tests PROPERTIES ${dbp_ubsan_probe_properties})
endif()

add_test(NAME dbp_tests COMMAND dbp_tests)
set_tests_properties(dbp_tests PROPERTIES ${dbp_test_properties})
