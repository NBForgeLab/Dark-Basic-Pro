include_guard(GLOBAL)

function(dbp_enable_parallel_msvc target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /FS)
    endif()
endfunction()

function(dbp_apply_legacy_cpp_options target)
    target_compile_features(${target} PRIVATE cxx_std_17)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /EHsc
            /W3
            /Zc:forScope-
        )
    endif()

    dbp_enable_parallel_msvc(${target})
endfunction()

function(dbp_apply_modern_cpp_options target)
    # The current tests include legacy headers whose public API accepts mutable
    # character pointers. Keep the compatibility suite on C++17 until those
    # APIs are made const-correct in the memory-safety phase.
    target_compile_features(${target} PRIVATE cxx_std_17)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /EHsc
            /W4
        )
    endif()

    dbp_enable_parallel_msvc(${target})
endfunction()
