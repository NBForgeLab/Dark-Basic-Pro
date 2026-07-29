#include "dbp/package/KeyProvider.h"
#include "dbp/package/PackageFormat.h"
#include "dbp/package/PackageReader.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

using namespace dbp::package;

int VerifyPackage(
    const std::filesystem::path& packagePath,
    const std::filesystem::path& keyPath) {
    std::error_code sizeError;
    const auto fileSize =
        std::filesystem::file_size(packagePath, sizeError);
    if (sizeError || fileSize < kPackageHeaderSize) {
        std::cerr << "package probe: input unavailable\n";
        return 3;
    }

    std::ifstream input(packagePath, std::ios::binary);
    std::vector<std::uint8_t> headerBytes(kPackageHeaderSize);
    input.read(
        reinterpret_cast<char*>(headerBytes.data()),
        static_cast<std::streamsize>(headerBytes.size()));
    if (input.gcount() !=
        static_cast<std::streamsize>(headerBytes.size())) {
        std::cerr << "package probe: header unavailable\n";
        return 3;
    }
    const auto header =
        ParsePackageHeader(headerBytes, fileSize, PackageLimits{});
    if (!header) {
        std::cerr << "package probe: package rejected\n";
        return 4;
    }

    FileKeyProvider keys(header.value().keyId, keyPath);
    CngCryptoProvider crypto;
    ZstdCompressionCodec compression;
    Win32AtomicFilePublisher publisher;
    const auto reader = PackageReader::Open(
        packagePath,
        keys,
        crypto,
        compression,
        publisher);
    if (!reader) {
        std::cerr << "package probe: package rejected\n";
        return 4;
    }
    for (const auto& record :
         reader.value()->manifest().records) {
        if (!reader.value()->ReadEntry(record.path)) {
            std::cerr << "package probe: payload rejected\n";
            return 4;
        }
    }
    return 0;
}

} // namespace

int wmain(const int argc, wchar_t* argv[]) {
    if (argc != 3) {
        std::cerr
            << "usage: dbp-package-probe <package.dbpak> <key-file>\n";
        return 2;
    }
    return VerifyPackage(argv[1], argv[2]);
}
