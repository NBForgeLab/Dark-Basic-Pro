#include "dbp/package/PackageReader.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

class ScopedInputFile {
public:
    ScopedInputFile(const std::uint8_t* data, const std::size_t size) {
        static std::atomic<std::uint64_t> sequence{0};
        path_ = std::filesystem::temp_directory_path() /
            ("dbp-package-fuzz-" +
             std::to_string(GetCurrentProcessId()) + "-" +
             std::to_string(sequence.fetch_add(
                 1,
                 std::memory_order_relaxed)) +
             ".dbpak");
        std::ofstream output(
            path_,
            std::ios::binary | std::ios::trunc);
        if (size != 0) {
            output.write(
                reinterpret_cast<const char*>(data),
                static_cast<std::streamsize>(size));
        }
        valid_ = static_cast<bool>(output);
    }

    ~ScopedInputFile() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    bool valid() const noexcept {
        return valid_;
    }

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
    bool valid_ = false;
};

} // namespace

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    const std::size_t size) {
    constexpr std::size_t maximumInput = 4U * 1024U * 1024U;
    if (size > maximumInput) {
        return 0;
    }
    ScopedInputFile input(data, size);
    if (!input.valid()) {
        return 0;
    }

    dbp::package::PackageLimits limits;
    limits.maximumEntries = 1'000;
    limits.maximumManifestSize = 1024U * 1024U;
    limits.maximumEntryPlaintextSize = maximumInput;
    limits.maximumTotalPlaintextSize = maximumInput;
    limits.maximumArchiveSize = maximumInput;

    dbp::package::KeyId keyId{};
    if (size >= dbp::package::kPackageHeaderSize) {
        const std::vector<std::uint8_t> headerBytes(
            data,
            data + dbp::package::kPackageHeaderSize);
        const auto header = dbp::package::ParsePackageHeader(
            headerBytes,
            size,
            limits);
        if (header) {
            keyId = header.value().keyId;
        }
    }

    std::vector<std::uint8_t> key(
        dbp::package::kPackageMasterKeySize,
        0);
    dbp::package::MemoryKeyProvider keys(
        keyId,
        dbp::package::SecureBuffer::FromBytes(key));
    dbp::package::CngCryptoProvider crypto;
    dbp::package::ZstdCompressionCodec compression;
    dbp::package::Win32AtomicFilePublisher publisher;
    const auto reader = dbp::package::PackageReader::Open(
        input.path(),
        keys,
        crypto,
        compression,
        publisher,
        limits);
    if (reader && !reader.value()->manifest().records.empty()) {
        (void)reader.value()->ReadEntry(
            reader.value()->manifest().records.front().path);
    }
    return 0;
}
