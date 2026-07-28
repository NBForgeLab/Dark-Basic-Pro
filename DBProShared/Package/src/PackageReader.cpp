#include "dbp/package/PackageReader.h"

#include "dbp/package/PackagePath.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

constexpr std::size_t ioBufferSize = 1024U * 1024U;

PackageError ReaderError(
    const PackageErrorCode code,
    std::string message) {
    return {
        code,
        std::move(message),
        std::nullopt,
    };
}

PackageError IoError(std::string message) {
    return ReaderError(
        PackageErrorCode::IoFailed,
        std::move(message));
}

PackageError FormatError(std::string message) {
    return ReaderError(
        PackageErrorCode::InvalidFormat,
        std::move(message));
}

PackageError PublicationError(std::string message) {
    return ReaderError(
        PackageErrorCode::PublicationFailed,
        std::move(message));
}

class TemporaryFiles {
public:
    ~TemporaryFiles() {
        for (auto path = paths_.rbegin();
             path != paths_.rend();
             ++path) {
            std::error_code ignored;
            std::filesystem::remove(*path, ignored);
        }
    }

    TemporaryFiles() = default;
    TemporaryFiles(const TemporaryFiles&) = delete;
    TemporaryFiles& operator=(const TemporaryFiles&) = delete;

    void Add(std::filesystem::path path) {
        paths_.push_back(std::move(path));
    }

private:
    std::vector<std::filesystem::path> paths_;
};

PackageResult<std::vector<std::uint8_t>> ReadExtent(
    std::ifstream& input,
    const std::uint64_t offset,
    const std::size_t size) {
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            IoError("Seeking within a package failed."));
    }
    std::vector<std::uint8_t> bytes(size);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() !=
        static_cast<std::streamsize>(bytes.size())) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            IoError("Reading a package extent failed."));
    }
    return PackageResult<std::vector<std::uint8_t>>::Success(
        std::move(bytes));
}

PackageResult<bool> CopyExtent(
    const std::filesystem::path& source,
    const std::uint64_t offset,
    const std::uint64_t size,
    const std::filesystem::path& destination) {
    std::ifstream input(source, std::ios::binary);
    std::ofstream output(
        destination,
        std::ios::binary | std::ios::trunc);
    if (!input || !output) {
        return PackageResult<bool>::Failure(
            IoError("Opening a private extraction stream failed."));
    }
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) {
        return PackageResult<bool>::Failure(
            IoError("Seeking to an encrypted payload failed."));
    }

    std::vector<std::uint8_t> buffer(ioBufferSize);
    std::uint64_t remaining = size;
    while (remaining != 0) {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(
                remaining,
                buffer.size()));
        input.read(
            reinterpret_cast<char*>(buffer.data()),
            static_cast<std::streamsize>(count));
        if (input.gcount() !=
            static_cast<std::streamsize>(count)) {
            return PackageResult<bool>::Failure(
                IoError("Reading an encrypted payload failed."));
        }
        output.write(
            reinterpret_cast<const char*>(buffer.data()),
            static_cast<std::streamsize>(count));
        if (!output) {
            return PackageResult<bool>::Failure(
                IoError("Writing a private ciphertext copy failed."));
        }
        remaining -= count;
    }
    output.flush();
    output.close();
    if (!output) {
        return PackageResult<bool>::Failure(
            IoError("Flushing a private ciphertext copy failed."));
    }
    return PackageResult<bool>::Success(true);
}

std::string Hex(const std::vector<std::uint8_t>& bytes) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : bytes) {
        output << std::setw(2) << static_cast<unsigned>(byte);
    }
    return output.str();
}

PackageResult<std::filesystem::path> CreatePrivateDirectory(
    const std::filesystem::path& parent,
    const CryptoProvider& crypto) {
    for (std::size_t attempt = 0; attempt < 8; ++attempt) {
        const auto random = crypto.RandomBytes(16);
        if (!random) {
            return PackageResult<std::filesystem::path>::Failure(
                random.error());
        }
        if (random.value().size() != 16) {
            return PackageResult<std::filesystem::path>::Failure(
                ReaderError(
                    PackageErrorCode::CryptographyFailed,
                    "The random provider returned an invalid size."));
        }
        const auto path =
            parent / (".dbpak-read-" + Hex(random.value()));
        std::error_code createError;
        if (std::filesystem::create_directory(path, createError)) {
            return PackageResult<std::filesystem::path>::Success(path);
        }
        if (createError) {
            return PackageResult<std::filesystem::path>::Failure(
                PublicationError(
                    "Creating a private extraction directory failed."));
        }
    }
    return PackageResult<std::filesystem::path>::Failure(
        PublicationError(
            "Creating a unique private extraction directory failed."));
}

PackageResult<bool> ValidateDestination(
    const std::filesystem::path& destination) {
    std::error_code existsError;
    if (std::filesystem::exists(destination, existsError) ||
        existsError) {
        return PackageResult<bool>::Failure(
            PublicationError(
                "The extraction destination already exists or "
                "cannot be inspected."));
    }
    auto parent = destination.parent_path();
    if (parent.empty()) {
        std::error_code currentPathError;
        parent = std::filesystem::current_path(currentPathError);
        if (currentPathError) {
            return PackageResult<bool>::Failure(
                PublicationError(
                    "Resolving the extraction destination directory failed."));
        }
    }
    std::error_code parentError;
    const auto parentStatus =
        std::filesystem::symlink_status(parent, parentError);
    if (parentError ||
        !std::filesystem::is_directory(parentStatus) ||
        std::filesystem::is_symlink(parentStatus)) {
        return PackageResult<bool>::Failure(
            PublicationError(
                "The extraction destination directory is unavailable "
                "or unsafe."));
    }
    return PackageResult<bool>::Success(true);
}

} // namespace

PackageReader::PackageReader(
    std::filesystem::path packagePath,
    PackageHeader header,
    PackageManifest manifest,
    SecureBuffer masterKey,
    const CryptoProvider& crypto,
    const ZstdCompressionCodec& compression,
    const AtomicFilePublisher& publisher,
    const PackageLimits limits) noexcept
    : packagePath_(std::move(packagePath)),
      header_(std::move(header)),
      manifest_(std::move(manifest)),
      masterKey_(std::move(masterKey)),
      crypto_(crypto),
      compression_(compression),
      publisher_(publisher),
      limits_(limits) {}

PackageResult<std::unique_ptr<PackageReader>> PackageReader::Open(
    const std::filesystem::path& packagePath,
    const KeyProvider& keys,
    const CryptoProvider& crypto,
    const ZstdCompressionCodec& compression,
    const AtomicFilePublisher& publisher,
    const PackageLimits& limits) {
    std::error_code absoluteError;
    const auto resolvedPackagePath =
        std::filesystem::absolute(packagePath, absoluteError)
            .lexically_normal();
    if (absoluteError) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            IoError("Resolving the package path failed."));
    }
    std::error_code statusError;
    const auto status =
        std::filesystem::symlink_status(
            resolvedPackagePath,
            statusError);
    if (statusError ||
        !std::filesystem::is_regular_file(status) ||
        std::filesystem::is_symlink(status)) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            IoError("The package file is unavailable or unsafe."));
    }
    std::error_code sizeError;
    const auto rawFileSize =
        std::filesystem::file_size(
            resolvedPackagePath,
            sizeError);
    if (sizeError ||
        rawFileSize >
            std::numeric_limits<std::uint64_t>::max()) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            IoError("Reading the package file size failed."));
    }
    const auto fileSize =
        static_cast<std::uint64_t>(rawFileSize);

    std::ifstream input(
        resolvedPackagePath,
        std::ios::binary);
    if (!input) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            IoError("Opening the package file failed."));
    }
    const auto headerBytes =
        ReadExtent(input, 0, kPackageHeaderSize);
    if (!headerBytes) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            headerBytes.error());
    }
    auto header = ParsePackageHeader(
        headerBytes.value(),
        fileSize,
        limits);
    if (!header) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            header.error());
    }

    const auto manifestEnd =
        header.value().manifestOffset +
        header.value().manifestCiphertextSize;
    const auto paddingSize =
        header.value().payloadOffset - manifestEnd;
    const auto padding = ReadExtent(
        input,
        manifestEnd,
        static_cast<std::size_t>(paddingSize));
    if (!padding) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            padding.error());
    }
    if (!std::all_of(
            padding.value().begin(),
            padding.value().end(),
            [](const std::uint8_t byte) { return byte == 0; })) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            FormatError("Package alignment padding must be zero."));
    }

    auto masterKey = keys.Resolve(header.value().keyId);
    if (!masterKey) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            masterKey.error());
    }
    const auto manifestCiphertext = ReadExtent(
        input,
        header.value().manifestOffset,
        static_cast<std::size_t>(
            header.value().manifestCiphertextSize));
    if (!manifestCiphertext) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            manifestCiphertext.error());
    }
    PackageKeyDeriver deriver(crypto);
    const auto manifestKey = deriver.DeriveManifestKey(
        masterKey.value(),
        header.value().packageId);
    if (!manifestKey) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            manifestKey.error());
    }
    const auto manifestPlaintext = crypto.Aes256GcmDecrypt(
        manifestKey.value(),
        header.value().manifestNonce,
        manifestCiphertext.value(),
        BuildManifestAdditionalData(header.value()),
        header.value().manifestTag);
    if (!manifestPlaintext) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            manifestPlaintext.error());
    }
    const auto manifestHash =
        crypto.Sha256(manifestPlaintext.value());
    if (!manifestHash ||
        manifestHash.value() !=
            header.value().manifestPlaintextSha256) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            ReaderError(
                PackageErrorCode::IntegrityFailed,
                "The authenticated manifest hash is invalid."));
    }
    auto manifest = ParseManifest(
        manifestPlaintext.value(),
        header.value(),
        limits);
    if (!manifest) {
        return PackageResult<std::unique_ptr<PackageReader>>::Failure(
            manifest.error());
    }
    for (const auto& record : manifest.value().records) {
        if (record.compression == CompressionAlgorithm::None &&
            record.storedSize != record.plaintextSize) {
            return PackageResult<std::unique_ptr<PackageReader>>::Failure(
                FormatError(
                    "An uncompressed entry has inconsistent sizes."));
        }
    }

    return PackageResult<std::unique_ptr<PackageReader>>::Success(
        std::unique_ptr<PackageReader>(new PackageReader(
            resolvedPackagePath,
            std::move(header.value()),
            std::move(manifest.value()),
            std::move(masterKey.value()),
            crypto,
            compression,
            publisher,
            limits)));
}

PackageResult<bool> PackageReader::ExtractEntry(
    const std::string_view packagePath,
    const std::filesystem::path& destination) const {
    const auto normalized =
        NormalizePackageInputPath(packagePath);
    if (!normalized) {
        return PackageResult<bool>::Failure(normalized.error());
    }
    const auto record = std::lower_bound(
        manifest_.records.begin(),
        manifest_.records.end(),
        normalized.value(),
        [](const ManifestRecord& candidate,
           const std::string& path) {
            return candidate.path < path;
        });
    if (record == manifest_.records.end() ||
        record->path != normalized.value()) {
        return PackageResult<bool>::Failure(
            FormatError("The requested package entry does not exist."));
    }
    const auto destinationValid =
        ValidateDestination(destination);
    if (!destinationValid) {
        return PackageResult<bool>::Failure(
            destinationValid.error());
    }
    auto parent = destination.parent_path();
    if (parent.empty()) {
        std::error_code currentPathError;
        parent = std::filesystem::current_path(currentPathError);
        if (currentPathError) {
            return PackageResult<bool>::Failure(
                PublicationError(
                    "Resolving the extraction destination directory failed."));
        }
    }
    const auto stagingDirectory =
        CreatePrivateDirectory(parent, crypto_);
    if (!stagingDirectory) {
        return PackageResult<bool>::Failure(
            stagingDirectory.error());
    }

    TemporaryFiles temporaryFiles;
    temporaryFiles.Add(stagingDirectory.value());
    const auto ciphertextPath =
        stagingDirectory.value() / "ciphertext.bin";
    const auto storedPlaintextPath =
        stagingDirectory.value() / "stored-plaintext.bin";
    const auto finalPlaintextPath =
        record->compression == CompressionAlgorithm::Zstandard
        ? stagingDirectory.value() / "plaintext.bin"
        : storedPlaintextPath;
    temporaryFiles.Add(ciphertextPath);
    temporaryFiles.Add(storedPlaintextPath);
    if (finalPlaintextPath != storedPlaintextPath) {
        temporaryFiles.Add(finalPlaintextPath);
    }

    const auto copied = CopyExtent(
        packagePath_,
        header_.payloadOffset + record->payloadOffset,
        record->storedSize,
        ciphertextPath);
    if (!copied) {
        return PackageResult<bool>::Failure(copied.error());
    }
    PackageKeyDeriver deriver(crypto_);
    const auto entryKey = deriver.DeriveEntryKey(
        masterKey_,
        header_.packageId,
        record->path);
    if (!entryKey) {
        return PackageResult<bool>::Failure(entryKey.error());
    }
    std::ifstream ciphertextInput(
        ciphertextPath,
        std::ios::binary);
    std::ofstream storedPlaintextOutput(
        storedPlaintextPath,
        std::ios::binary | std::ios::trunc);
    if (!ciphertextInput || !storedPlaintextOutput) {
        return PackageResult<bool>::Failure(
            IoError("Opening private decryption streams failed."));
    }
    const auto decrypted = crypto_.Aes256GcmDecryptStream(
        entryKey.value(),
        record->nonce,
        ciphertextInput,
        storedPlaintextOutput,
        BuildEntryAdditionalData(header_.packageId, *record),
        record->tag,
        record->storedSize);
    ciphertextInput.close();
    storedPlaintextOutput.close();
    if (!decrypted || !storedPlaintextOutput) {
        return PackageResult<bool>::Failure(
            decrypted
                ? IoError("Closing private plaintext output failed.")
                : decrypted.error());
    }

    if (record->compression == CompressionAlgorithm::Zstandard) {
        std::ifstream compressedInput(
            storedPlaintextPath,
            std::ios::binary);
        std::ofstream plaintextOutput(
            finalPlaintextPath,
            std::ios::binary | std::ios::trunc);
        if (!compressedInput || !plaintextOutput) {
            return PackageResult<bool>::Failure(
                IoError("Opening private decompression streams failed."));
        }
        const auto decompressed = compression_.DecompressStream(
            compressedInput,
            plaintextOutput,
            record->plaintextSize,
            std::min(
                limits_.maximumEntryPlaintextSize,
                record->plaintextSize));
        compressedInput.close();
        plaintextOutput.close();
        if (!decompressed || !plaintextOutput) {
            return PackageResult<bool>::Failure(
                decompressed
                    ? IoError(
                        "Closing private decompression output failed.")
                    : decompressed.error());
        }
    }

    std::ifstream hashInput(
        finalPlaintextPath,
        std::ios::binary);
    if (!hashInput) {
        return PackageResult<bool>::Failure(
            IoError("Opening private plaintext for hashing failed."));
    }
    const auto digest =
        crypto_.Sha256Stream(hashInput, record->plaintextSize);
    hashInput.close();
    if (!digest ||
        digest.value().digest != record->plaintextSha256) {
        return PackageResult<bool>::Failure(
            digest
                ? ReaderError(
                    PackageErrorCode::IntegrityFailed,
                    "The package entry plaintext hash is invalid.")
                : digest.error());
    }

    const auto published =
        publisher_.Publish(finalPlaintextPath, destination);
    if (!published) {
        return PackageResult<bool>::Failure(published.error());
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<std::shared_ptr<const std::vector<std::uint8_t>>>
PackageReader::ReadEntry(const std::string_view packagePath) const {
    const auto normalized =
        NormalizePackageInputPath(packagePath);
    if (!normalized) {
        return PackageResult<
            std::shared_ptr<const std::vector<std::uint8_t>>>::Failure(
                normalized.error());
    }
    const auto record = std::lower_bound(
        manifest_.records.begin(),
        manifest_.records.end(),
        normalized.value(),
        [](const ManifestRecord& candidate,
           const std::string& path) {
            return candidate.path < path;
        });
    if (record == manifest_.records.end() ||
        record->path != normalized.value()) {
        return PackageResult<
            std::shared_ptr<const std::vector<std::uint8_t>>>::Failure(
                FormatError(
                    "The requested package entry does not exist."));
    }
    if (record->storedSize >
            (std::numeric_limits<std::size_t>::max)() ||
        record->plaintextSize >
            (std::numeric_limits<std::size_t>::max)()) {
        return PackageResult<
            std::shared_ptr<const std::vector<std::uint8_t>>>::Failure(
                ReaderError(
                    PackageErrorCode::LimitExceeded,
                    "The package entry is too large for an in-memory "
                    "read on this process architecture."));
    }

    std::ifstream input(packagePath_, std::ios::binary);
    if (!input) {
        return PackageResult<
            std::shared_ptr<const std::vector<std::uint8_t>>>::Failure(
                IoError("Opening the package payload failed."));
    }
    const auto ciphertext = ReadExtent(
        input,
        header_.payloadOffset + record->payloadOffset,
        static_cast<std::size_t>(record->storedSize));
    if (!ciphertext) {
        return PackageResult<
            std::shared_ptr<const std::vector<std::uint8_t>>>::Failure(
                ciphertext.error());
    }
    PackageKeyDeriver deriver(crypto_);
    const auto entryKey = deriver.DeriveEntryKey(
        masterKey_,
        header_.packageId,
        record->path);
    if (!entryKey) {
        return PackageResult<
            std::shared_ptr<const std::vector<std::uint8_t>>>::Failure(
                entryKey.error());
    }
    auto stored = crypto_.Aes256GcmDecrypt(
        entryKey.value(),
        record->nonce,
        ciphertext.value(),
        BuildEntryAdditionalData(header_.packageId, *record),
        record->tag);
    if (!stored) {
        return PackageResult<
            std::shared_ptr<const std::vector<std::uint8_t>>>::Failure(
                stored.error());
    }

    std::vector<std::uint8_t> plaintext;
    if (record->compression == CompressionAlgorithm::Zstandard) {
        CompressedBuffer compressed{
            CompressionAlgorithm::Zstandard,
            std::move(stored.value()),
        };
        auto decompressed = compression_.Decompress(
            compressed,
            record->plaintextSize,
            (std::min)(
                limits_.maximumEntryPlaintextSize,
                record->plaintextSize));
        if (!decompressed) {
            return PackageResult<
                std::shared_ptr<const std::vector<std::uint8_t>>>::Failure(
                    decompressed.error());
        }
        plaintext = std::move(decompressed.value());
    } else {
        plaintext = std::move(stored.value());
    }
    if (plaintext.size() != record->plaintextSize) {
        return PackageResult<
            std::shared_ptr<const std::vector<std::uint8_t>>>::Failure(
                ReaderError(
                    PackageErrorCode::IntegrityFailed,
                    "The package entry plaintext size is invalid."));
    }
    const auto digest = crypto_.Sha256(plaintext);
    if (!digest ||
        digest.value() != record->plaintextSha256) {
        return PackageResult<
            std::shared_ptr<const std::vector<std::uint8_t>>>::Failure(
                digest
                    ? ReaderError(
                        PackageErrorCode::IntegrityFailed,
                        "The package entry plaintext hash is invalid.")
                    : digest.error());
    }
    return PackageResult<
        std::shared_ptr<const std::vector<std::uint8_t>>>::Success(
            std::make_shared<const std::vector<std::uint8_t>>(
                std::move(plaintext)));
}

} // namespace dbp::package
