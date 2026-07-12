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
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/CInputC.cpp"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input/Controller.cpp"
    "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/System/dynamic_call.asm"
)

dbp_apply_modern_cpp_options(dbp_tests)

target_link_libraries(dbp_tests PRIVATE
    dbp_compiler_lib
    GTest::gtest
    spdlog::spdlog
    xinput
)

target_compile_definitions(dbp_tests PRIVATE DBP_TESTS_COMPILATION)

target_include_directories(dbp_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPCompiler"
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
