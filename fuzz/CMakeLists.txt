cmake_minimum_required(VERSION 3.25)

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
