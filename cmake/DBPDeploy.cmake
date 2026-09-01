# DBPDeploy.cmake — build-integrated deployment into the FPS Creator Classic
# checkout. Included twice from the top-level CMakeLists.txt:
#
#   set(DBP_DEPLOY_PHASE SETTINGS) / include(...)  — before all subprojects:
#       resolves the checkout root, derives the deploy paths, and defines
#       dbp_deploy_target(), which target directories call to attach
#       POST_BUILD sync hooks.
#   set(DBP_DEPLOY_PHASE TARGETS)  / include(...)  — after all subprojects:
#       defines the aggregate workflow targets.
#
# The historical workflow copied DBPCompiler.exe, DBProCore.dll, and freshly
# packaged .dbpak artifacts into the FPSC checkout by hand. Hand-copies drift:
# a test then silently exercises a binary that predates the fix, producing
# irreproducible crashes and wasted diagnosis. Deployment is therefore a
# build product here, never a manual step:
#
#   POST_BUILD hooks  The compiler, debugger, and core runtime DLL deploy
#                     themselves into the checkout right after linking; any
#                     ordinary build of these targets deploys them.
#   fpsc-deploy       Builds every deployable target (compiler, debugger, all
#                     shared runtime plugins) and syncs each output into the
#                     checkout. The single entry point for a full deployment.
#   fpsc-verify       Audits the checkout against the build tree by SHA-256
#                     and fails loudly on any drift (stale or missing copy).
#   fpsc-package      Recompiles the FPSC-MapEditor project with the freshly
#                     deployed compiler and syncs the resulting exe/dbpakref/
#                     dbpak trio into Files/.
#
# Deployment is on by default whenever the sibling FPS Creator Classic
# checkout is detected; disable with -DDBP_DEPLOY_TO_FPSC=OFF or point
# DBP_FPSC_ROOT at another checkout.

if(NOT DEFINED DBP_DEPLOY_PHASE)
    message(FATAL_ERROR
        "DBPDeploy.cmake must be included with DBP_DEPLOY_PHASE set to "
        "SETTINGS (before subprojects) or TARGETS (after subprojects)")
endif()

# ---------------------------------------------------------------------------
# SETTINGS phase — options, checkout discovery, deploy paths, hook helper.
# ---------------------------------------------------------------------------
if(DBP_DEPLOY_PHASE STREQUAL "SETTINGS")
    option(DBP_DEPLOY_TO_FPSC
        "Deploy build outputs into the FPS Creator Classic checkout" ON)
    set(DBP_FPSC_ROOT "" CACHE PATH
        "FPSC deploy root (.../Dark Basic Pro Shared/Dark Basic Pro); auto-detected when empty")

    # Executables deployed next to the FPSCreator-facing compiler installation.
    set(DBP_FPSC_COMPILER_EXE_TARGETS DBPCompiler DBPDebugger)

    set(DBP_FPSC_DEPLOY_ENABLED FALSE)

    if(DBP_DEPLOY_TO_FPSC)
        if(NOT DBP_FPSC_ROOT)
            # The git common directory resolves to the primary repository even
            # when this checkout is a worktree, so the workspace root stays
            # correct in both layouts.
            execute_process(
                COMMAND git rev-parse --path-format=absolute --git-common-dir
                WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
                OUTPUT_VARIABLE DBP_FPSC_GIT_COMMON_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(DBP_FPSC_GIT_COMMON_DIR)
                get_filename_component(DBP_FPSC_REPO_ROOT "${DBP_FPSC_GIT_COMMON_DIR}" DIRECTORY)
                get_filename_component(DBP_FPSC_WORKSPACE_ROOT "${DBP_FPSC_REPO_ROOT}" DIRECTORY)
            else()
                get_filename_component(DBP_FPSC_WORKSPACE_ROOT "${PROJECT_SOURCE_DIR}" DIRECTORY)
            endif()
            set(DBP_FPSC_CANDIDATE
                "${DBP_FPSC_WORKSPACE_ROOT}/FPS-Creator-Classic/Dark Basic Pro Shared/Dark Basic Pro")
            if(EXISTS "${DBP_FPSC_CANDIDATE}/Compiler"
               AND EXISTS "${DBP_FPSC_CANDIDATE}/Projects/FPSCREATOR")
                # FORCE is required: the cache entry already exists (as "")
                # and a plain CACHE set would silently keep the empty value.
                set(DBP_FPSC_ROOT "${DBP_FPSC_CANDIDATE}" CACHE PATH
                    "FPSC deploy root (.../Dark Basic Pro Shared/Dark Basic Pro); auto-detected when empty"
                    FORCE)
            endif()
        endif()

        if(DBP_FPSC_ROOT
           AND EXISTS "${DBP_FPSC_ROOT}/Compiler"
           AND EXISTS "${DBP_FPSC_ROOT}/Projects/FPSCREATOR")
            set(DBP_FPSC_DEPLOY_ENABLED TRUE)
        else()
            message(STATUS
                "FPSC deployment disabled: no FPS Creator Classic checkout "
                "found (set DBP_FPSC_ROOT or disable DBP_DEPLOY_TO_FPSC)")
        endif()
    endif()

    if(DBP_FPSC_DEPLOY_ENABLED)
        set(DBP_FPSC_COMPILER_DIR "${DBP_FPSC_ROOT}/Compiler")
        set(DBP_FPSC_PROJECT_DIR "${DBP_FPSC_ROOT}/Projects/FPSCREATOR")
        set(DBP_FPSC_FILES_DIR "${DBP_FPSC_PROJECT_DIR}/Files")
        set(DBP_FPSC_COMPILER_PLUGINS_DIR "${DBP_FPSC_COMPILER_DIR}/plugins")
        set(DBP_FPSC_FILES_PLUGINS_DIR "${DBP_FPSC_FILES_DIR}/plugins")
        message(STATUS "FPSC deployment enabled: ${DBP_FPSC_ROOT}")
    endif()

    # Attaches a POST_BUILD hook deploying ${target} into the checkout. Must
    # be called from the directory that creates the target (CMake restricts
    # add_custom_command(TARGET) to that directory).
    function(dbp_deploy_target target)
        if(NOT DBP_FPSC_DEPLOY_ENABLED)
            return()
        endif()

        get_target_property(target_type ${target} TYPE)
        set(dest_dirs "")
        if(target_type STREQUAL "EXECUTABLE"
           AND target IN_LIST DBP_FPSC_COMPILER_EXE_TARGETS)
            set(dest_dirs "${DBP_FPSC_COMPILER_DIR}")
        elseif(target_type STREQUAL "SHARED_LIBRARY")
            set(dest_dirs
                "${DBP_FPSC_COMPILER_PLUGINS_DIR}"
                "${DBP_FPSC_FILES_PLUGINS_DIR}")
            if(target STREQUAL "DBProCore")
                # The packaged editor also loads the core DLL from the Files
                # root, so that historical copy must stay in sync as well.
                list(APPEND dest_dirs "${DBP_FPSC_FILES_DIR}")
            endif()
        endif()

        if(NOT dest_dirs)
            message(AUTHOR_WARNING
                "dbp_deploy_target(${target}): target is not deployable")
            return()
        endif()

        set(deploy_commands "")
        foreach(dest_dir IN LISTS dest_dirs)
            list(APPEND deploy_commands
                COMMAND ${CMAKE_COMMAND} -E make_directory "${dest_dir}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${target}>" "${dest_dir}")
            set_property(GLOBAL APPEND PROPERTY DBP_FPSC_MANIFEST
                "$<TARGET_FILE:${target}>|${dest_dir}/$<TARGET_FILE_NAME:${target}>")
        endforeach()

        add_custom_command(TARGET ${target} POST_BUILD
            ${deploy_commands}
            VERBATIM
            COMMENT "Deploying ${target} into the FPSC checkout")
        set_property(GLOBAL APPEND PROPERTY DBP_FPSC_DEPLOYED_TARGETS ${target})
    endfunction()
    return()
endif()

# ---------------------------------------------------------------------------
# TARGETS phase — manifest generation and aggregate workflow targets.
# ---------------------------------------------------------------------------
if(NOT DBP_DEPLOY_PHASE STREQUAL "TARGETS")
    message(FATAL_ERROR "unknown DBP_DEPLOY_PHASE '${DBP_DEPLOY_PHASE}'")
endif()

function(dbp_collect_directory_targets dir out_var)
    get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(subdir IN LISTS subdirs)
        dbp_collect_directory_targets("${subdir}" subdir_targets)
        list(APPEND targets ${subdir_targets})
    endforeach()
    set(${out_var} "${targets}" PARENT_SCOPE)
endfunction()

set(DBP_FPSC_ALL_DEPLOYABLE_TARGETS "")

if(DBP_FPSC_DEPLOY_ENABLED)
    dbp_collect_directory_targets("${CMAKE_SOURCE_DIR}" DBP_FPSC_ALL_TARGETS)
    if(DBP_FPSC_ALL_TARGETS)
        list(REMOVE_DUPLICATES DBP_FPSC_ALL_TARGETS)
    endif()

    get_property(dbp_hooked_targets GLOBAL PROPERTY DBP_FPSC_DEPLOYED_TARGETS)

    set(dbp_deploy_commands "")
    foreach(target IN LISTS DBP_FPSC_ALL_TARGETS)
        get_target_property(target_type ${target} TYPE)

        set(dest_dirs "")
        if(target_type STREQUAL "EXECUTABLE"
           AND target IN_LIST DBP_FPSC_COMPILER_EXE_TARGETS)
            set(dest_dirs "${DBP_FPSC_COMPILER_DIR}")
        elseif(target_type STREQUAL "SHARED_LIBRARY")
            set(dest_dirs
                "${DBP_FPSC_COMPILER_PLUGINS_DIR}"
                "${DBP_FPSC_FILES_PLUGINS_DIR}")
            if(target STREQUAL "DBProCore")
                list(APPEND dest_dirs "${DBP_FPSC_FILES_DIR}")
            endif()
        endif()

        if(NOT dest_dirs)
            continue()
        endif()

        list(APPEND DBP_FPSC_ALL_DEPLOYABLE_TARGETS ${target})

        # Targets that already carry a POST_BUILD hook keep their manifest
        # entries; every target gets an explicit copy step here as well, so
        # fpsc-deploy stays a complete, self-contained sync.
        foreach(dest_dir IN LISTS dest_dirs)
            if(NOT target IN_LIST dbp_hooked_targets)
                list(APPEND dbp_deploy_commands
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${dest_dir}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:${target}>" "${dest_dir}")
            endif()
            set_property(GLOBAL APPEND PROPERTY DBP_FPSC_MANIFEST
                "$<TARGET_FILE:${target}>|${dest_dir}/$<TARGET_FILE_NAME:${target}>")
        endforeach()
    endforeach()

    # Deduplicate manifest lines (hooked targets contribute once via the
    # SETTINGS phase; guard above prevents double entries here).
    get_property(DBP_FPSC_MANIFEST_LINES GLOBAL PROPERTY DBP_FPSC_MANIFEST)
    if(DBP_FPSC_MANIFEST_LINES)
        list(REMOVE_DUPLICATES DBP_FPSC_MANIFEST_LINES)
    endif()
    string(JOIN "\n" DBP_FPSC_MANIFEST_CONTENT ${DBP_FPSC_MANIFEST_LINES})
    file(GENERATE
        OUTPUT "${CMAKE_BINARY_DIR}/fpsc_deploy_manifest_$<CONFIG>.txt"
        CONTENT "${DBP_FPSC_MANIFEST_CONTENT}\n")

    add_custom_target(fpsc-verify
        COMMAND ${CMAKE_COMMAND}
            "-DMODE=verify"
            "-DMANIFEST=${CMAKE_BINARY_DIR}/fpsc_deploy_manifest_$<CONFIG>.txt"
            -P "${CMAKE_CURRENT_LIST_DIR}/dbp_deploy_sync.cmake"
        COMMENT "Auditing the FPSC checkout against the build tree"
        VERBATIM)

    set(DBP_FPSC_PACKAGE_KEY_FILE "$ENV{LOCALAPPDATA}/Temp/fpsc_package.key"
        CACHE FILEPATH "Package key used to sign FPSC .dbpak files")
    set(DBP_FPSC_MAPEDITOR_PROJECT "FPSC-MapEditor (english).dbpro"
        CACHE STRING "FPSC project re-packaged by fpsc-package")
    set(DBP_FPSC_MAPEDITOR_NAME "FPSC-MapEditor"
        CACHE STRING "Artifact base name produced for the packaged project")

    if(NOT EXISTS "${DBP_FPSC_PACKAGE_KEY_FILE}")
        message(STATUS
            "FPSC package key not found at ${DBP_FPSC_PACKAGE_KEY_FILE}; "
            "fpsc-package will fail until the key is provided "
            "(-DDBP_FPSC_PACKAGE_KEY_FILE=...)")
    endif()

    add_custom_target(fpsc-package
        DEPENDS DBPCompiler DBProCore
        COMMAND "${DBP_FPSC_COMPILER_DIR}/DBPCompiler.exe"
            --json
            --package-key-file "${DBP_FPSC_PACKAGE_KEY_FILE}"
            "${DBP_FPSC_MAPEDITOR_PROJECT}"
        COMMAND ${CMAKE_COMMAND}
            "-DMODE=package"
            "-DFPSC_PROJECT_DIR=${DBP_FPSC_PROJECT_DIR}"
            "-DFPSC_FILES_DIR=${DBP_FPSC_FILES_DIR}"
            "-DFPSC_ARTIFACT_NAME=${DBP_FPSC_MAPEDITOR_NAME}"
            -P "${CMAKE_CURRENT_LIST_DIR}/dbp_deploy_sync.cmake"
        WORKING_DIRECTORY "${DBP_FPSC_PROJECT_DIR}"
        COMMENT "Re-packaging ${DBP_FPSC_MAPEDITOR_PROJECT} and syncing the trio into Files/"
        VERBATIM)

    list(LENGTH DBP_FPSC_ALL_DEPLOYABLE_TARGETS DBP_FPSC_DEPLOYED_COUNT)
    message(STATUS
        "FPSC deployment ready: ${DBP_FPSC_DEPLOYED_COUNT} deployable targets "
        "(targets: fpsc-deploy, fpsc-verify, fpsc-package)")
endif()

# Aggregate entry point: build every deployable target and sync all outputs
# into the checkout. Defined even when deployment is disabled so build
# presets can reference it unconditionally (it then only builds).
add_custom_target(fpsc-deploy
    ${dbp_deploy_commands}
    DEPENDS ${DBP_FPSC_ALL_DEPLOYABLE_TARGETS}
    COMMENT "Deploying all build outputs into the FPSC checkout"
    VERBATIM)
