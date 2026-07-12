include_guard(GLOBAL)

option(DBP_ENABLE_ASAN "Enable AddressSanitizer for project-owned C/C++ targets" OFF)

function(dbp_enable_parallel_msvc target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:C,CXX>:/FS>
        )
    endif()
endfunction()

function(dbp_enable_sanitizers target)
    if(NOT DBP_ENABLE_ASAN)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:C,CXX>:/fsanitize=address>
            $<$<COMPILE_LANGUAGE:C,CXX>:/Oy->
        )

        get_target_property(target_type ${target} TYPE)
        if(target_type STREQUAL "EXECUTABLE" OR
           target_type STREQUAL "SHARED_LIBRARY" OR
           target_type STREQUAL "MODULE_LIBRARY")
            target_link_options(${target} PRIVATE
                /INCREMENTAL:NO
            )
        endif()
    else()
        message(FATAL_ERROR "DBP_ENABLE_ASAN currently supports the MSVC compatibility build only")
    endif()
endfunction()

function(dbp_apply_legacy_cpp_options target)
    target_compile_features(${target} PRIVATE cxx_std_17)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/EHa>
            $<$<COMPILE_LANGUAGE:C,CXX>:/W3>
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:forScope->
        )
    endif()

    dbp_enable_parallel_msvc(${target})
    dbp_enable_sanitizers(${target})
endfunction()

function(dbp_apply_modern_cpp_options target)
    # The current tests include legacy headers whose public API accepts mutable
    # character pointers. Keep the compatibility suite on C++17 until those
    # APIs are made const-correct in the memory-safety phase.
    target_compile_features(${target} PRIVATE cxx_std_17)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/EHa>
            $<$<COMPILE_LANGUAGE:C,CXX>:/W4>
        )
    endif()

    dbp_enable_parallel_msvc(${target})
    dbp_enable_sanitizers(${target})
endfunction()
