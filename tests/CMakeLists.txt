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
    test_target_abi.cpp
    test_x64_assembler.cpp
    test_x64_intensive_verification.cpp
    test_plugin_build_matrix.cpp
    test_executable_preparation_pipeline.cpp
    test_asmwriter_preparation_services.cpp
    test_prepared_executable_debugger.cpp
    test_prepared_executable_packager.cpp
    test_asmwriter_prepare_exe.cpp
    test_modern_cpp_safety.cpp
    test_exeblock_memory.cpp
    test_exeblock_arrays.cpp
    test_exeblock_wx.cpp
    test_exeblock_x64.cpp
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
        test_redim_multidimensional.cpp
        test_array_memory_safety.cpp
        test_array_index_bounds.cpp
        test_array_direct_layout.cpp
        test_stack_queue_bounds.cpp
        test_statement_userfunction.cpp
        test_statement_assignment.cpp
        test_statement_token.cpp
        test_statement_data.cpp
        test_statement_paramlist.cpp
        test_statement_expression.cpp
    test_asmwriter_emission.cpp
    test_leap_marker_manager.cpp
    test_debugger_interface.cpp
    test_debugger_protocol.cpp
    test_machine_code_buffer.cpp
    test_reference_tracker.cpp
    test_tokenizer.cpp
    test_statement_tokenizer_delegation.cpp
    test_expression_parser.cpp
    test_expression_parser_symbols.cpp
    test_task_emitter.cpp
    test_task_emitter_passes.cpp
    test_pe_builder.cpp
    test_pe_builder_headers.cpp
    test_pe_builder_generation.cpp
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
    test_data_type_enum_values.cpp
    test_instruction_table_enum_values.cpp
    test_asmwriter_enum_values.cpp
    test_clang_tidy_integration.cpp
    test_coverage_integration.cpp
    test_install_rules.cpp
    test_e2e_x64_compilation.cpp
    test_x64_intensive_verification.cpp

    # --- Codegen verification suite (compiler + assembly generator) ---------
    # Shared in-process driver that runs the real emission pipeline without
    # touching disk, then captures bytes / listing / references.
    codegen/CodegenHarness.cpp
    # Golden-file regression net over tests/codegen/goldens.
    codegen/test_codegen_golden.cpp
    # Invariant checks that must hold for any generated program.
    codegen/test_codegen_oracles.cpp
    # Boundary and hostile inputs.
    codegen/test_codegen_edge_cases.cpp
    # Unattended soak: heap / handle / determinism hygiene.
    codegen/test_codegen_headless.cpp
    # Per-instruction and per-construct codegen matrices.
    codegen/test_codegen_instruction_matrix.cpp
    codegen/test_codegen_language_matrix.cpp
    # A UDT array element's string member is an 8-byte pointer on x64; the
    # compiler-generated deep clone must move it at pointer width.
    codegen/test_codegen_udt_array_string_width.cpp

    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPCompilerEXE/RuntimePackageBootstrap.cpp"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/CInputC.cpp"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/Controller.cpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}"  # tests/ root: lets codegen/*.cpp include "codegen/CodegenHarness.h"
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPCompiler"
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPDebugger"
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPCompilerEXE"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${DXSDK_INCLUDE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"  # generated InstallConfig.h
)

# Generate InstallConfig.h from template so tests can verify CMake config
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/InstallConfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/InstallConfig.h"
    @ONLY
)

if(MSVC)
    # Same thread-stack contract as the production compiler binaries
    # (DBPCompiler / DarkEXE link with /STACK:33554432,1048576): the legacy
    # emission pipeline needs far more than the default 1MB, and with the
    # default the suite intermittently exhausts the stack and faults inside
    # whatever frame touches the guard page.
    target_link_options(dbp_tests PRIVATE /SAFESEH:NO "/STACK:33554432,1048576")
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

# ---------------------------------------------------------------------------
# Codegen fuzz-corpus tooling (coverage expansion)
# ---------------------------------------------------------------------------
# NOTE: the deterministic corpus generator (dbp_codegen_seed_generator) is NOT
# defined here. It must be buildable on every toolchain and in the Clang fuzzing
# configuration too, so it lives — exactly once — in fuzz/CMakeLists.txt, outside
# that file's Clang-only libFuzzer guard. Defining it here as well made CMake
# fail with a duplicate target whenever BUILD_TESTING and DBP_BUILD_FUZZERS were
# both enabled.

# Standalone per-input driver: compiles one .dba file via the in-process harness
# and enforces the universal codegen contract. Launched in isolation by
# run_codegen_tests.py so a crash in one input cannot mask the others.
add_executable(dbp_codegen_corpus_runner
    codegen/CodegenCorpusRunner.cpp
    codegen/CodegenHarness.cpp
)
target_compile_features(dbp_codegen_corpus_runner PRIVATE cxx_std_17)
target_link_libraries(dbp_codegen_corpus_runner PRIVATE
    dbp_compiler_lib
    spdlog::spdlog
)
target_compile_definitions(dbp_codegen_corpus_runner PRIVATE
    DBP_TESTS_COMPILATION
    DBP_TEST_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
)
target_include_directories(dbp_codegen_corpus_runner PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPCompiler"
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPDebugger"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
)
if(MSVC)
    target_link_options(dbp_codegen_corpus_runner PRIVATE /SAFESEH:NO "/STACK:33554432,1048576")
endif()
