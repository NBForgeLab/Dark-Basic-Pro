add_library(dbp_package STATIC
    src/AtomicFilePublisher.cpp
    src/ByteCodec.cpp
    src/CngCryptoProvider.cpp
    src/ExecutableKeyResource.cpp
    src/KeyProvider.cpp
    src/LegacyPckReader.cpp
    src/PackageFormat.cpp
    src/PackagePath.cpp
    src/PackageReader.cpp
    src/PackageWriter.cpp
    src/RuntimeDescriptor.cpp
    src/SecureBuffer.cpp
    src/ZstdCompressionCodec.cpp
)

dbp_apply_modern_cpp_options(dbp_package)

target_include_directories(dbp_package
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(dbp_package
    PRIVATE
        bcrypt
        libzstd_static
)
