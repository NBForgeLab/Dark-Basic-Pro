add_library(dbp_package STATIC
    src/ByteCodec.cpp
    src/CngCryptoProvider.cpp
    src/PackageFormat.cpp
    src/PackagePath.cpp
    src/SecureBuffer.cpp
)

dbp_apply_modern_cpp_options(dbp_package)

target_include_directories(dbp_package
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(dbp_package
    PRIVATE
        bcrypt
)
