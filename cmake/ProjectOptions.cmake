include_guard(GLOBAL)

option(DBP_ENABLE_ASAN "Enable AddressSanitizer for project-owned C/C++ targets" OFF)
option(DBP_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for project-owned C/C++ targets" OFF)
option(DBP_ENABLE_COVERAGE "Enable code coverage instrumentation for project-owned C/C++ targets" OFF)
option(DBP_BUILD_FUZZERS "Build optional Clang/libFuzzer package targets" OFF)

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

function(dbp_enable_ubsan target)
    if(NOT DBP_ENABLE_UBSAN)
        return()
    endif()

    if(MSVC)
        # MSVC 17.6+ (cl 19.36+) is the minimum version for UBSan support.
        # Current MSVC silently ignores /fsanitize=undefined (D9002).
        # We still record the intent; when MSVC ships real UBSan the flag
        # can be uncommented.
        if(MSVC_VERSION LESS 1936)
            message(STATUS "MSVC ${MSVC_VERSION} < 19.36 — /fsanitize=undefined not available, UBSan skipped for ${target}")
            return()
        endif()
        message(STATUS "UBSan requested but MSVC does not yet implement /fsanitize=undefined — probe tests will SKIP at runtime [${target}]")
    else()
        target_compile_options(${target} PRIVATE
            -fsanitize=undefined -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE
            -fsanitize=undefined
        )
    endif()
endfunction()

function(dbp_enable_coverage target)
    if(NOT DBP_ENABLE_COVERAGE)
        return()
    endif()

    if(MSVC)
        # MSVC 17.1+ (cl 19.29+) supports /coverage for code instrumentation.
        # Older versions silently ignore the flag, so we gate on MSVC_VERSION.
        if(MSVC_VERSION LESS 1929)
            message(STATUS "MSVC ${MSVC_VERSION} < 19.29 — /coverage not available, skipped for ${target}")
            return()
        endif()
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:C,CXX>:/coverage>
        )
        target_link_options(${target} PRIVATE
            /coverage
        )
    else()
        # GCC / Clang
        target_compile_options(${target} PRIVATE
            --coverage -fprofile-arcs -ftest-coverage
        )
        target_link_options(${target} PRIVATE
            --coverage
        )
    endif()
endfunction()

function(dbp_apply_legacy_cpp_options target)
    # All compiler targets build as native x64, standards-conformant C++20
    # with high warning visibility and strict conformance mode (/permissive-).
    # Legacy SDK sources are held to the same ISO rules as modern code; any
    # pre-standard constructs must be fixed at the source, not via flags.
    target_compile_features(${target} PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/EHa>
            $<$<COMPILE_LANGUAGE:C,CXX>:/W4>
            $<$<COMPILE_LANGUAGE:C,CXX>:/permissive->
        )
    endif()

    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "SHARED_LIBRARY" OR target_type STREQUAL "MODULE_LIBRARY")
        set_target_properties(${target} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()

    dbp_enable_parallel_msvc(${target})
    dbp_enable_sanitizers(${target})
    dbp_enable_ubsan(${target})
    dbp_enable_coverage(${target})
endfunction()

function(dbp_apply_modern_cpp_options target)
    # Strict ISO conformance (/permissive-) is mandatory for modern targets;
    # several C++20 features (concepts, two-phase lookup) require it.
    target_compile_features(${target} PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/EHa>
            $<$<COMPILE_LANGUAGE:C,CXX>:/W4>
            $<$<COMPILE_LANGUAGE:C,CXX>:/permissive->
        )
    endif()

    dbp_enable_parallel_msvc(${target})
    dbp_enable_sanitizers(${target})
    dbp_enable_ubsan(${target})
    dbp_enable_coverage(${target})
endfunction()
