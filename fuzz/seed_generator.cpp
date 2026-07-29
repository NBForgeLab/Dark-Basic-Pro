#include "dbp/package/ByteCodec.h"
#include "dbp/package/PackageWriter.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

bool WriteBytes(
    const std::filesystem::path& path,
    const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    output.close();
    return static_cast<bool>(output);
}

std::vector<std::uint8_t> ReadBytes(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
}

std::vector<std::uint8_t> LegacySeed() {
    constexpr std::uint32_t executableSize = 64;
    constexpr std::uint32_t validityCode = 12345678;
    const std::string path = "_virtual.dat";
    const std::vector<std::uint8_t> payload{1, 2, 3, 4};
    dbp::package::ByteWriter writer;
    std::vector<std::uint8_t> executable(executableSize, 0x90);
    executable[0] = 'M';
    executable[1] = 'Z';
    writer.WriteBytes(executable.data(), executable.size());
    writer.WriteUInt32(static_cast<std::uint32_t>(path.size()));
    writer.WriteBytes(
        reinterpret_cast<const std::uint8_t*>(path.data()),
        path.size());
    writer.WriteUInt32(static_cast<std::uint32_t>(payload.size()));
    writer.WriteBytes(payload.data(), payload.size());
    writer.WriteUInt32(0);
    writer.WriteUInt32(0);
    writer.WriteUInt32(validityCode);
    writer.WriteUInt32(0);
    writer.WriteUInt32(executableSize);
    return writer.Bytes();
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        return 2;
    }
    const auto v2Directory = std::filesystem::path(argv[1]);
    const auto legacyDirectory = std::filesystem::path(argv[2]);
    std::error_code error;
    std::filesystem::create_directories(v2Directory, error);
    std::filesystem::create_directories(legacyDirectory, error);
    if (error) {
        return 3;
    }

    const auto source = v2Directory / "seed-source.bin";
    if (!WriteBytes(source, {0x10, 0x20, 0x30, 0x40})) {
        return 4;
    }
    dbp::package::KeyId keyId{};
    std::vector<std::uint8_t> key(
        dbp::package::kPackageMasterKeySize,
        0);
    dbp::package::MemoryKeyProvider keys(
        keyId,
        dbp::package::SecureBuffer::FromBytes(key));
    dbp::package::CngCryptoProvider crypto;
    dbp::package::ZstdCompressionCodec compression;
    dbp::package::Win32AtomicFilePublisher publisher;
    dbp::package::PackageWriter writer(
        crypto,
        compression,
        publisher);
    const auto package = writer.Write(
        {
            v2Directory,
            keyId,
            {{source, "_virtual.dat", true}},
        },
        keys);
    if (!package) {
        return 5;
    }
    const auto validV2 = ReadBytes(package.value().packagePath);
    if (!WriteBytes(v2Directory / "valid.dbpak", validV2)) {
        return 6;
    }
    const std::vector<std::uint8_t> malformedV2(
        validV2.begin(),
        validV2.begin() +
            std::min<std::size_t>(validV2.size(), 31));
    if (!WriteBytes(
            v2Directory / "malformed-truncated.dbpak",
            malformedV2)) {
        return 7;
    }
    std::filesystem::remove(source, error);
    std::filesystem::remove(package.value().packagePath, error);

    const auto validLegacy = LegacySeed();
    if (!WriteBytes(
            legacyDirectory / "valid.exe",
            validLegacy) ||
        !WriteBytes(
            legacyDirectory / "malformed-truncated.exe",
            std::vector<std::uint8_t>(
                validLegacy.begin(),
                validLegacy.begin() + 15))) {
        return 8;
    }
    return 0;
}
