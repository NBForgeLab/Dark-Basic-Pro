# dbp_deploy_sync.cmake — audit and packaging helper for DBPDeploy.cmake.
#
#   cmake -DMODE=verify -DMANIFEST=<file> -P dbp_deploy_sync.cmake
#       Audits every "build output|deployed copy" pair in the manifest by
#       SHA-256 and fails when any deployed copy is stale or missing. Never
#       writes. The manifest is generated per configuration by DBPDeploy.cmake.
#
#   cmake -DMODE=package -DFPSC_PROJECT_DIR=<dir>
#         -DFPSC_ARTIFACT_NAME=<base> -P dbp_deploy_sync.cmake
#       Proves the freshly compiled project trio (exe, dbpakref, and the dbpak
#       the dbpakref names) is complete in the project directory. Never writes:
#       the compiler runs with that directory as its working directory, so it
#       already emits the trio where the FPSC parent resolves the child from.

function(dbp_fail message)
    message(FATAL_ERROR "[dbp-deploy] ${message}")
endfunction()

function(dbp_hex_pair_value pair out_var)
    set(digits "0123456789abcdef")
    string(TOLOWER "${pair}" pair)
    string(SUBSTRING "${pair}" 0 1 high)
    string(SUBSTRING "${pair}" 1 1 low)
    string(FIND "${digits}" "${high}" high_value)
    string(FIND "${digits}" "${low}" low_value)
    if(high_value LESS 0 OR low_value LESS 0)
        dbp_fail("invalid hex byte '${pair}'")
    endif()
    math(EXPR value "${high_value} * 16 + ${low_value}")
    set(${out_var} "${value}" PARENT_SCOPE)
endfunction()

function(dbp_hex_to_ascii hex out_var)
    string(LENGTH "${hex}" hex_length)
    math(EXPR remainder "${hex_length} % 2")
    if(NOT remainder EQUAL 0)
        dbp_fail("odd-length hex string")
    endif()
    set(result "")
    set(index 0)
    while(index LESS hex_length)
        string(SUBSTRING "${hex}" ${index} 2 pair)
        dbp_hex_pair_value("${pair}" code)
        string(ASCII ${code} char)
        string(APPEND result "${char}")
        math(EXPR index "${index} + 2")
    endwhile()
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# DBPREF2 layout (little-endian):
#   0x00  "DBPREF2\0" magic
#   0x08  u64 entry count
#   0x10  32 bytes of identifiers
#   0x30  u32 name length, followed by the NUL-terminated dbpak file name
function(dbp_parse_dbpakref ref_path out_var)
    file(READ "${ref_path}" ref_hex HEX)
    string(LENGTH "${ref_hex}" ref_hex_length)
    if(ref_hex_length LESS 106)
        dbp_fail("dbpakref '${ref_path}' is too small to be a DBPREF2 blob")
    endif()
    string(SUBSTRING "${ref_hex}" 0 16 magic_hex)
    if(NOT magic_hex STREQUAL "4442505245463200")
        dbp_fail("dbpakref '${ref_path}' is not a DBPREF2 blob (magic '${magic_hex}')")
    endif()
    set(name_length 0)
    set(shift 1)
    foreach(byte_index 0 1 2 3)
        math(EXPR hex_offset "96 + ${byte_index} * 2")
        string(SUBSTRING "${ref_hex}" ${hex_offset} 2 pair)
        dbp_hex_pair_value("${pair}" byte_value)
        math(EXPR name_length "${name_length} + ${byte_value} * ${shift}")
        math(EXPR shift "${shift} * 256")
    endforeach()
    if(name_length LESS 1 OR name_length GREATER 255)
        dbp_fail("dbpakref '${ref_path}' carries an implausible name length ${name_length}")
    endif()
    math(EXPR required "104 + ${name_length} * 2")
    if(ref_hex_length LESS required)
        dbp_fail("dbpakref '${ref_path}' is truncated inside its file name")
    endif()
    math(EXPR name_hex_length "${name_length} * 2")
    string(SUBSTRING "${ref_hex}" 104 ${name_hex_length} name_hex)
    dbp_hex_to_ascii("${name_hex}" name)
    set(${out_var} "${name}" PARENT_SCOPE)
endfunction()

function(dbp_require_file expected_file label)
    if(NOT EXISTS "${expected_file}")
        dbp_fail("expected ${label} is missing: ${expected_file}")
    endif()
    message(STATUS "[dbp-deploy] ${label}: ${expected_file}")
endfunction()

if(NOT DEFINED MODE)
    dbp_fail("MODE must be set (verify|package)")
endif()

if(MODE STREQUAL "verify")
    if(NOT DEFINED MANIFEST OR NOT EXISTS "${MANIFEST}")
        dbp_fail("manifest not found ('${MANIFEST}'); configure with FPSC deployment enabled first")
    endif()
    file(STRINGS "${MANIFEST}" entries)
    set(total 0)
    set(bad 0)
    foreach(entry IN LISTS entries)
        if(NOT entry MATCHES "^([^|]+)\\|(.+)$")
            continue()
        endif()
        set(source_file "${CMAKE_MATCH_1}")
        set(dest_file "${CMAKE_MATCH_2}")
        math(EXPR total "${total} + 1")
        if(NOT EXISTS "${source_file}")
            set(status "BUILD-OUTPUT-MISSING")
        elseif(NOT EXISTS "${dest_file}")
            set(status "DEPLOY-MISSING")
        else()
            file(SHA256 "${source_file}" source_hash)
            file(SHA256 "${dest_file}" dest_hash)
            if(source_hash STREQUAL dest_hash)
                set(status "OK")
            else()
                set(status "STALE")
            endif()
        endif()
        if(NOT status STREQUAL "OK")
            math(EXPR bad "${bad} + 1")
        endif()
        message(STATUS "[verify] ${status}: ${dest_file}")
    endforeach()
    if(total EQUAL 0)
        dbp_fail("manifest '${MANIFEST}' contains no entries")
    endif()
    if(bad GREATER 0)
        dbp_fail("${bad}/${total} deployed artifacts are stale or missing; build the fpsc-deploy target")
    endif()
    message(STATUS "[verify] all ${total} deployed artifacts match the current build")
elseif(MODE STREQUAL "package")
    if(NOT DEFINED FPSC_PROJECT_DIR)
        dbp_fail("FPSC_PROJECT_DIR must be set for package mode")
    endif()
    if(NOT DEFINED FPSC_ARTIFACT_NAME)
        set(FPSC_ARTIFACT_NAME "FPSC-MapEditor")
    endif()
    if(NOT IS_DIRECTORY "${FPSC_PROJECT_DIR}")
        dbp_fail("project directory does not exist: ${FPSC_PROJECT_DIR}")
    endif()
    set(exe_file "${FPSC_PROJECT_DIR}/${FPSC_ARTIFACT_NAME}.exe")
    set(ref_file "${FPSC_PROJECT_DIR}/${FPSC_ARTIFACT_NAME}.dbpakref")

    # The parent launches the child as lpFile = <name>.exe with lpDirectory =
    # the project directory (EditorDoc.cpp:298-300), so the trio belongs here
    # and nowhere else. Files/ is the game-data root the editor reads media
    # from; a second exe copy there is never loaded and only drifts.
    dbp_require_file("${exe_file}" "executable")
    dbp_require_file("${ref_file}" "reference")
    dbp_parse_dbpakref("${ref_file}" dbpak_name)
    if(NOT dbpak_name MATCHES "^data-[0-9a-fA-F]+\\.dbpak$")
        dbp_fail("dbpakref references an unexpected file name '${dbpak_name}'")
    endif()
    dbp_require_file("${FPSC_PROJECT_DIR}/${dbpak_name}" "package")
    message(STATUS "[dbp-deploy] dbpakref references ${dbpak_name}")

    if(DEFINED FPSC_EDITOR_FILES_DIR AND EXISTS "${FPSC_EDITOR_FILES_DIR}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${exe_file}" "${FPSC_EDITOR_FILES_DIR}/${FPSC_ARTIFACT_NAME}.exe"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ref_file}" "${FPSC_EDITOR_FILES_DIR}/${FPSC_ARTIFACT_NAME}.dbpakref"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FPSC_PROJECT_DIR}/${dbpak_name}" "${FPSC_EDITOR_FILES_DIR}/${dbpak_name}"
        )
        message(STATUS "[dbp-deploy] synced trio to ${FPSC_EDITOR_FILES_DIR}")
    endif()
else()
    dbp_fail("unknown MODE '${MODE}' (expected verify|package)")
endif()
