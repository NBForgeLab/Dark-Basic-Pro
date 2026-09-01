# ---------------------------------------------------------------------------
# Codegen corpus seed generator
# ---------------------------------------------------------------------------
# A plain host tool: no sanitizers, no libFuzzer, any toolchain. It is defined
# here and ONLY here so that both the MSVC corpus workflow (BUILD_TESTING=ON)
# and the Clang fuzzing workflow (DBP_BUILD_FUZZERS=ON) build the very same
# binary. It must stay outside the Clang-only guard below.
#
# (It used to also be defined in tests/CMakeLists.txt, which made configuring
# with both BUILD_TESTING=ON and DBP_BUILD_FUZZERS=ON fail with a duplicate
# target error.)
add_executable(dbp_codegen_seed_generator codegen_seed_generator.cpp)
target_compile_features(dbp_codegen_seed_generator PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# libFuzzer targets — Clang + libFuzzer only, opt-in via DBP_BUILD_FUZZERS
# ---------------------------------------------------------------------------
if(DBP_BUILD_FUZZERS)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
        "DBP_BUILD_FUZZERS requires Clang with libFuzzer support")
endif()

function(dbp_add_package_fuzzer target source)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_options(${target} PRIVATE
        -fsanitize=fuzzer,address
        -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE
        -fsanitize=fuzzer,address
    )
    target_link_libraries(${target} PRIVATE dbp_package)
endfunction()

dbp_add_package_fuzzer(
    dbp_package_reader_fuzzer
    package_reader_fuzzer.cpp
)
dbp_add_package_fuzzer(
    dbp_legacy_pck_reader_fuzzer
    legacy_pck_reader_fuzzer.cpp
)

add_executable(dbp_fuzz_seed_generator seed_generator.cpp)
target_compile_features(dbp_fuzz_seed_generator PRIVATE cxx_std_17)
target_link_libraries(dbp_fuzz_seed_generator PRIVATE dbp_package)

# ---------------------------------------------------------------------------
# Codegen fuzzing layer (coverage expansion)
# ---------------------------------------------------------------------------
# libFuzzer entry that drives CompileSnippet() over the corpus and aborts on any
# universal-contract violation (unresolved branch, buffer canary overrun).
function(dbp_add_codegen_fuzzer target source)
    add_executable(${target} ${source}
        "${PROJECT_SOURCE_DIR}/tests/codegen/CodegenHarness.cpp")
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_options(${target} PRIVATE
        -fsanitize=fuzzer,address
        -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE
        -fsanitize=fuzzer,address
    )
    target_link_libraries(${target} PRIVATE dbp_compiler_lib spdlog::spdlog)
    target_include_directories(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/tests"
        "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPCompiler"
        "${PROJECT_SOURCE_DIR}/DBProCompiler/DBPDebugger"
        "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Input"
        "${PROJECT_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    )
    target_compile_definitions(${target} PRIVATE
        DBP_TEST_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
    )
endfunction()

dbp_add_codegen_fuzzer(
    dbp_codegen_fuzzer
    dbp_codegen_fuzzer.cpp
)

endif()  # DBP_BUILD_FUZZERS
