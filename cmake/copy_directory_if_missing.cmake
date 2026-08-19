# copy_directory_if_missing.cmake
#
# Deploys a legacy host-runtime directory next to a freshly built target, but
# never overwrites files that the build already produced. The committed
# Install/Compiler tree still carries historical prebuilt plug-ins; those are
# fallbacks for binaries the current build cannot produce, never a source that
# may shadow a fresh build.
#
# Usage:
#   cmake -DDBP_COPY_SOURCE_DIR=<dir> -DDBP_COPY_DEST_DIR=<dir>
#         -P cmake/copy_directory_if_missing.cmake

if(NOT DEFINED DBP_COPY_SOURCE_DIR OR NOT DEFINED DBP_COPY_DEST_DIR)
    message(FATAL_ERROR
        "DBP_COPY_SOURCE_DIR and DBP_COPY_DEST_DIR must be provided")
endif()

if(NOT IS_DIRECTORY "${DBP_COPY_SOURCE_DIR}")
    message(FATAL_ERROR
        "DBP_COPY_SOURCE_DIR is not a directory: ${DBP_COPY_SOURCE_DIR}")
endif()

file(MAKE_DIRECTORY "${DBP_COPY_DEST_DIR}")

file(GLOB_RECURSE entries RELATIVE "${DBP_COPY_SOURCE_DIR}"
     "${DBP_COPY_SOURCE_DIR}/*")

foreach(entry IN LISTS entries)
    set(sourcePath "${DBP_COPY_SOURCE_DIR}/${entry}")
    set(destPath "${DBP_COPY_DEST_DIR}/${entry}")
    if(IS_DIRECTORY "${sourcePath}")
        file(MAKE_DIRECTORY "${destPath}")
    elseif(NOT EXISTS "${destPath}")
        file(COPY "${sourcePath}" DESTINATION "${DBP_COPY_DEST_DIR}")
    endif()
endforeach()
