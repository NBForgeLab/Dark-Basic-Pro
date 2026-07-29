#include "dbp/package/PackageWriter.h"
#include "dbp/package/PackageReader.h"

#include "dbp/package/ByteCodec.h"
#include "dbp/package/PackagePath.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

constexpr std::size_t ioBufferSize = 1024U * 1024U;

PackageError WriterError(
    const PackageErrorCode code,
    std::string message) {
    return {
        code,
        std::move(message),
        std::nullopt,
    };
}

PackageError IoError(std::string message) {
    return WriterError(
        PackageErrorCode::IoFailed,
        std::move(message));
}

PackageError PublicationError(std::string message) {
    return WriterError(
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

    TemporaryFiles(const TemporaryFiles&) = delete;
    TemporaryFiles& operator=(const TemporaryFiles&) = delete;
    TemporaryFiles() = default;

    void Add(std::filesystem::path path) {
        paths_.push_back(std::move(path));
    }

private:
    std::vector<std::filesystem::path> paths_;
};

class SourceFileHandle {
public:
    explicit SourceFileHandle(const HANDLE handle) noexcept
        : handle_(handle) {}

    ~SourceFileHandle() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    SourceFileHandle(const SourceFileHandle&) = delete;
    SourceFileHandle& operator=(const SourceFileHandle&) = delete;

    HANDLE get() const noexcept {
        return handle_;
    }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

PackageResult<PackageSourceIdentity> ReadSourceIdentity(
    const HANDLE handle) {
    BY_HANDLE_FILE_INFORMATION information{};
    FILE_BASIC_INFO basicInformation{};
    if (!GetFileInformationByHandle(handle, &information) ||
        !GetFileInformationByHandleEx(
            handle,
            FileBasicInfo,
            &basicInformation,
            sizeof(basicInformation)) ||
        (information.dwFileAttributes &
            (FILE_ATTRIBUTE_DIRECTORY |
             FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
        return PackageResult<PackageSourceIdentity>::Failure(
            IoError("A package source is unavailable or unsafe."));
    }
    PackageSourceIdentity identity;
    identity.volumeSerialNumber =
        information.dwVolumeSerialNumber;
    identity.fileIndex =
        (static_cast<std::uint64_t>(
             information.nFileIndexHigh) << 32U) |
        information.nFileIndexLow;
    identity.size =
        (static_cast<std::uint64_t>(
             information.nFileSizeHigh) << 32U) |
        information.nFileSizeLow;
    identity.lastWriteTime =
        static_cast<std::uint64_t>(
            basicInformation.LastWriteTime.QuadPart);
    identity.changeTime =
        static_cast<std::uint64_t>(
            basicInformation.ChangeTime.QuadPart);
    return PackageResult<PackageSourceIdentity>::Success(identity);
}

struct ValidatedEntry {
    std::filesystem::path sourcePath;
    std::string packagePath;
    bool enableCompression = true;
    std::uint64_t size = 0;
    PackageSourceIdentity expectedIdentity;
    AesGcmNonce nonce{};
};

struct StagedEntry {
    std::filesystem::path storedPath;
    ManifestRecord record;
};

std::string Hex(const PackageId& id) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : id) {
        output << std::setw(2) << static_cast<unsigned>(byte);
    }
    return output.str();
}

std::string StageSuffix(const std::size_t index) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(6) << index;
    return output.str();
}

template <std::size_t Size>
PackageResult<std::array<std::uint8_t, Size>> RandomArray(
    const CryptoProvider& crypto) {
    const auto bytes = crypto.RandomBytes(Size);
    if (!bytes) {
        return PackageResult<std::array<std::uint8_t, Size>>::Failure(
            bytes.error());
    }
    if (bytes.value().size() != Size) {
        return PackageResult<std::array<std::uint8_t, Size>>::Failure(
            WriterError(
                PackageErrorCode::CryptographyFailed,
                "The cryptographic random provider returned an invalid size."));
    }
    std::array<std::uint8_t, Size> result{};
    std::copy(bytes.value().begin(), bytes.value().end(), result.begin());
    return PackageResult<std::array<std::uint8_t, Size>>::Success(result);
}

PackageResult<std::vector<ValidatedEntry>> ValidateRequest(
    const PackageWriteRequest& request) {
    std::error_code statusError;
    const auto outputStatus =
        std::filesystem::symlink_status(
            request.outputDirectory,
            statusError);
    if (statusError ||
        !std::filesystem::is_directory(outputStatus) ||
        std::filesystem::is_symlink(outputStatus)) {
        return PackageResult<std::vector<ValidatedEntry>>::Failure(
            IoError(
                "The package output directory is unavailable or unsafe."));
    }
    if (request.entries.size() > request.limits.maximumEntries) {
        return PackageResult<std::vector<ValidatedEntry>>::Failure(
            WriterError(
                PackageErrorCode::LimitExceeded,
                "The package entry count exceeds the configured limit."));
    }

    std::vector<ValidatedEntry> validated;
    validated.reserve(request.entries.size());
    std::vector<std::string> paths;
    paths.reserve(request.entries.size());
    std::uint64_t totalSize = 0;
    for (const auto& entry : request.entries) {
        const auto packagePath =
            NormalizePackageInputPath(entry.packagePath);
        if (!packagePath) {
            return PackageResult<std::vector<ValidatedEntry>>::Failure(
                packagePath.error());
        }
        if (packagePath.value().size() >
            request.limits.maximumPathBytes) {
            return PackageResult<std::vector<ValidatedEntry>>::Failure(
                WriterError(
                    PackageErrorCode::LimitExceeded,
                    "A package path exceeds the configured limit."));
        }

        const auto sourceIdentity =
            CapturePackageSourceIdentity(entry.sourcePath);
        if (!sourceIdentity) {
            return PackageResult<std::vector<ValidatedEntry>>::Failure(
                sourceIdentity.error());
        }
        if (entry.expectedIdentity &&
            entry.expectedIdentity.value() !=
                sourceIdentity.value()) {
            return PackageResult<std::vector<ValidatedEntry>>::Failure(
                IoError(
                    "A package source changed after it was selected."));
        }
        const auto size = sourceIdentity.value().size;
        if (size > request.limits.maximumEntryPlaintextSize) {
            return PackageResult<std::vector<ValidatedEntry>>::Failure(
                WriterError(
                    PackageErrorCode::LimitExceeded,
                    "A package source exceeds the configured size limit."));
        }
        const auto nextTotal = CheckedAdd(totalSize, size);
        if (!nextTotal ||
            nextTotal.value() >
                request.limits.maximumTotalPlaintextSize) {
            return PackageResult<std::vector<ValidatedEntry>>::Failure(
                WriterError(
                    PackageErrorCode::LimitExceeded,
                    "Package sources exceed the configured total-size limit."));
        }
        totalSize = nextTotal.value();

        paths.push_back(packagePath.value());
        validated.push_back({
            entry.sourcePath,
            packagePath.value(),
            entry.enableCompression,
            size,
            sourceIdentity.value(),
            {},
        });
    }

    const auto canonicalPaths = ValidateAndSortPackagePaths(paths);
    if (!canonicalPaths) {
        return PackageResult<std::vector<ValidatedEntry>>::Failure(
            canonicalPaths.error());
    }
    std::sort(
        validated.begin(),
        validated.end(),
        [](const ValidatedEntry& left, const ValidatedEntry& right) {
            return left.packagePath < right.packagePath;
        });
    return PackageResult<std::vector<ValidatedEntry>>::Success(
        std::move(validated));
}

PackageResult<std::uint64_t> CopySourceSnapshot(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& snapshotPath,
    const PackageSourceIdentity& expectedIdentity) {
    SourceFileHandle input(CreateFileW(
        sourcePath.c_str(),
        GENERIC_READ | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT |
            FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr));
    if (input.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<std::uint64_t>::Failure(
            IoError("Opening a package source snapshot failed."));
    }
    const auto openedIdentity =
        ReadSourceIdentity(input.get());
    if (!openedIdentity ||
        openedIdentity.value() != expectedIdentity) {
        return PackageResult<std::uint64_t>::Failure(
            openedIdentity
                ? IoError(
                    "A package source changed while it was being staged.")
                : openedIdentity.error());
    }

    std::ofstream output(
        snapshotPath,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        return PackageResult<std::uint64_t>::Failure(
            IoError("Opening a package source snapshot failed."));
    }

    const auto expectedSize = expectedIdentity.size;
    auto buffer = SecureBuffer::FromBytes(
        std::vector<std::uint8_t>(ioBufferSize));
    std::uint64_t total = 0;
    while (true) {
        DWORD bytesRead = 0;
        if (!ReadFile(
                input.get(),
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead,
                nullptr)) {
            return PackageResult<std::uint64_t>::Failure(
                IoError("Reading a package source failed."));
        }
        if (bytesRead == 0) {
            break;
        }
        const auto count = static_cast<std::uint64_t>(bytesRead);
        if (total > expectedSize ||
            count > expectedSize - total) {
            return PackageResult<std::uint64_t>::Failure(
                IoError(
                    "A package source changed while it was being staged."));
        }
        output.write(
            reinterpret_cast<const char*>(buffer.data()),
            static_cast<std::streamsize>(bytesRead));
        if (!output) {
            return PackageResult<std::uint64_t>::Failure(
                IoError("Writing a package source snapshot failed."));
        }
        total += count;
    }
    const auto completedIdentity =
        ReadSourceIdentity(input.get());
    if (total != expectedSize ||
        !completedIdentity ||
        completedIdentity.value() != expectedIdentity) {
        return PackageResult<std::uint64_t>::Failure(
            IoError(
                "A package source changed while it was being staged."));
    }
    output.flush();
    output.close();
    if (!output) {
        return PackageResult<std::uint64_t>::Failure(
            IoError("Flushing a package source snapshot failed."));
    }
    return PackageResult<std::uint64_t>::Success(total);
}

PackageResult<bool> RemoveTemporaryFile(
    const std::filesystem::path& path) {
    std::error_code removeError;
    if (!std::filesystem::remove(path, removeError) ||
        removeError) {
        return PackageResult<bool>::Failure(
            IoError("Removing a private staging file failed."));
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<bool> EnsureNewPath(
    const std::filesystem::path& path,
    const PackageErrorCode errorCode,
    const char* const message) {
    std::error_code existsError;
    if (std::filesystem::exists(path, existsError) ||
        existsError) {
        return PackageResult<bool>::Failure(
            WriterError(errorCode, message));
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<bool> WriteZeros(
    std::ostream& output,
    std::uint64_t size) {
    const std::array<char, 64 * 1024> zeros{};
    while (size != 0) {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(size, zeros.size()));
        output.write(
            zeros.data(),
            static_cast<std::streamsize>(count));
        if (!output) {
            return PackageResult<bool>::Failure(
                IoError("Writing package layout placeholders failed."));
        }
        size -= count;
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<std::vector<std::uint8_t>> ReadExtent(
    std::ifstream& input,
    const std::uint64_t offset,
    const std::size_t size) {
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            IoError("Seeking while verifying a package failed."));
    }
    std::vector<std::uint8_t> bytes(size);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() !=
        static_cast<std::streamsize>(bytes.size())) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            IoError("Reading while verifying a package failed."));
    }
    return PackageResult<std::vector<std::uint8_t>>::Success(
        std::move(bytes));
}

PackageResult<bool> VerifyWrittenPackage(
    const std::filesystem::path& path,
    const PackageHeader& expectedHeader,
    const SecureBuffer& masterKey,
    const PackageLimits& limits,
    const CryptoProvider& crypto,
    const ZstdCompressionCodec& compression,
    const AtomicFilePublisher& publisher) {
    std::error_code sizeError;
    const auto fileSize =
        std::filesystem::file_size(path, sizeError);
    if (sizeError) {
        return PackageResult<bool>::Failure(
            IoError("Reading the completed package size failed."));
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return PackageResult<bool>::Failure(
            IoError("Reopening the completed package failed."));
    }

    const auto headerBytes =
        ReadExtent(input, 0, kPackageHeaderSize);
    if (!headerBytes) {
        return PackageResult<bool>::Failure(headerBytes.error());
    }
    const auto header = ParsePackageHeader(
        headerBytes.value(),
        fileSize,
        limits);
    if (!header ||
        header.value().packageId != expectedHeader.packageId ||
        header.value().keyId != expectedHeader.keyId) {
        return PackageResult<bool>::Failure(
            WriterError(
                PackageErrorCode::IntegrityFailed,
                "The completed package header failed verification."));
    }

    const auto manifestBytes = ReadExtent(
        input,
        header.value().manifestOffset,
        static_cast<std::size_t>(
            header.value().manifestCiphertextSize));
    if (!manifestBytes) {
        return PackageResult<bool>::Failure(manifestBytes.error());
    }
    PackageKeyDeriver deriver(crypto);
    const auto manifestKey = deriver.DeriveManifestKey(
        masterKey,
        header.value().packageId);
    if (!manifestKey) {
        return PackageResult<bool>::Failure(manifestKey.error());
    }
    const auto manifestPlaintext = crypto.Aes256GcmDecrypt(
        manifestKey.value(),
        header.value().manifestNonce,
        manifestBytes.value(),
        BuildManifestAdditionalData(header.value()),
        header.value().manifestTag);
    if (!manifestPlaintext) {
        return PackageResult<bool>::Failure(
            manifestPlaintext.error());
    }
    const auto manifestHash =
        crypto.Sha256(manifestPlaintext.value());
    if (!manifestHash ||
        manifestHash.value() !=
            header.value().manifestPlaintextSha256) {
        return PackageResult<bool>::Failure(
            WriterError(
                PackageErrorCode::IntegrityFailed,
                "The completed package manifest hash failed verification."));
    }
    const auto manifest = ParseManifest(
        manifestPlaintext.value(),
        header.value(),
        limits);
    if (!manifest) {
        return PackageResult<bool>::Failure(manifest.error());
    }

    const auto manifestEnd = CheckedAdd(
        header.value().manifestOffset,
        header.value().manifestCiphertextSize);
    if (!manifestEnd ||
        manifestEnd.value() > header.value().payloadOffset) {
        return PackageResult<bool>::Failure(
            WriterError(
                PackageErrorCode::IntegrityFailed,
                "The completed package layout failed verification."));
    }
    const auto paddingSize =
        header.value().payloadOffset - manifestEnd.value();
    const auto padding = ReadExtent(
        input,
        manifestEnd.value(),
        static_cast<std::size_t>(paddingSize));
    if (!padding ||
        !std::all_of(
            padding.value().begin(),
            padding.value().end(),
            [](const std::uint8_t byte) { return byte == 0; })) {
        return PackageResult<bool>::Failure(
            WriterError(
                PackageErrorCode::IntegrityFailed,
                "The completed package padding failed verification."));
    }

    MemoryKeyProvider keys(
        expectedHeader.keyId,
        SecureBuffer::FromBytes(masterKey.CopyBytes()));
    const auto verifiedReader = PackageReader::Open(
        path,
        keys,
        crypto,
        compression,
        publisher,
        limits);
    if (!verifiedReader ||
        verifiedReader.value()->header().packageId !=
            expectedHeader.packageId ||
        verifiedReader.value()->header().keyId !=
            expectedHeader.keyId) {
        return PackageResult<bool>::Failure(
            verifiedReader
                ? WriterError(
                    PackageErrorCode::IntegrityFailed,
                    "The production reader rejected the completed "
                    "package identity.")
                : verifiedReader.error());
    }
    for (const auto& record :
         verifiedReader.value()->manifest().records) {
        const auto plaintext =
            verifiedReader.value()->ReadEntry(record.path);
        if (!plaintext) {
            return PackageResult<bool>::Failure(
                plaintext.error());
        }
    }
    return PackageResult<bool>::Success(true);
}

} // namespace

PackageResult<PackageSourceIdentity>
CapturePackageSourceIdentity(
    const std::filesystem::path& sourcePath) {
    SourceFileHandle handle(CreateFileW(
        sourcePath.c_str(),
        GENERIC_READ | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT |
            FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<PackageSourceIdentity>::Failure(
            IoError("Opening a package source failed."));
    }
    return ReadSourceIdentity(handle.get());
}

PackageWriter::PackageWriter(
    const CryptoProvider& crypto,
    const ZstdCompressionCodec& compression,
    const AtomicFilePublisher& publisher) noexcept
    : crypto_(crypto),
      compression_(compression),
      publisher_(publisher) {}

PackageResult<PackageWriteResult> PackageWriter::Write(
    const PackageWriteRequest& request,
    const KeyProvider& keys) const {
    auto validated = ValidateRequest(request);
    if (!validated) {
        return PackageResult<PackageWriteResult>::Failure(
            validated.error());
    }

    const auto masterKey = keys.Resolve(request.keyId);
    if (!masterKey) {
        return PackageResult<PackageWriteResult>::Failure(
            masterKey.error());
    }
    const auto packageId = RandomArray<16>(crypto_);
    if (!packageId) {
        return PackageResult<PackageWriteResult>::Failure(
            packageId.error());
    }
    const auto packageHex = Hex(packageId.value());
    const auto finalPath = request.outputDirectory /
        ("data-" + packageHex + ".dbpak");
    const auto finalAvailable = EnsureNewPath(
        finalPath,
        PackageErrorCode::PublicationFailed,
        "The immutable package destination already exists.");
    if (!finalAvailable) {
        return PackageResult<PackageWriteResult>::Failure(
            finalAvailable.error());
    }

    const auto manifestNonce = RandomArray<12>(crypto_);
    if (!manifestNonce) {
        return PackageResult<PackageWriteResult>::Failure(
            manifestNonce.error());
    }
    std::set<AesGcmNonce> usedNonces;
    usedNonces.insert(manifestNonce.value());
    for (auto& entry : validated.value()) {
        const auto nonce = RandomArray<12>(crypto_);
        if (!nonce) {
            return PackageResult<PackageWriteResult>::Failure(
                nonce.error());
        }
        if (!usedNonces.insert(nonce.value()).second) {
            return PackageResult<PackageWriteResult>::Failure(
                WriterError(
                    PackageErrorCode::CryptographyFailed,
                    "The random provider repeated a package nonce."));
        }
        entry.nonce = nonce.value();
    }

    TemporaryFiles temporaryFiles;
    const auto stagingDirectory =
        request.outputDirectory /
        (".dbpak-stage-" + packageHex);
    std::error_code stagingError;
    if (!std::filesystem::create_directory(
            stagingDirectory,
            stagingError) ||
        stagingError) {
        return PackageResult<PackageWriteResult>::Failure(
            PublicationError(
                "Creating the private package staging directory failed."));
    }
    temporaryFiles.Add(stagingDirectory);

    std::vector<StagedEntry> staged;
    staged.reserve(validated.value().size());
    std::uint64_t payloadSize = 0;
    bool hasCompressedEntry = false;
    for (std::size_t index = 0;
         index < validated.value().size();
         ++index) {
        const auto& entry = validated.value()[index];
        const auto prefix = StageSuffix(index);
        const auto snapshotPath =
            stagingDirectory / (prefix + ".raw");
        const auto snapshotAvailable = EnsureNewPath(
            snapshotPath,
            PackageErrorCode::PublicationFailed,
            "A private package staging path already exists.");
        if (!snapshotAvailable) {
            return PackageResult<PackageWriteResult>::Failure(
                snapshotAvailable.error());
        }
        temporaryFiles.Add(snapshotPath);
        const auto copied = CopySourceSnapshot(
            entry.sourcePath,
            snapshotPath,
            entry.expectedIdentity);
        if (!copied) {
            return PackageResult<PackageWriteResult>::Failure(
                copied.error());
        }

        std::ifstream hashInput(snapshotPath, std::ios::binary);
        if (!hashInput) {
            return PackageResult<PackageWriteResult>::Failure(
                IoError("Opening a staged source for hashing failed."));
        }
        const auto hash =
            crypto_.Sha256Stream(hashInput, entry.size);
        hashInput.close();
        if (!hash) {
            return PackageResult<PackageWriteResult>::Failure(
                hash.error());
        }

        std::filesystem::path storedPath = snapshotPath;
        std::uint64_t storedSize = entry.size;
        CompressionAlgorithm algorithm = CompressionAlgorithm::None;
        if (entry.enableCompression) {
            const auto compressedPath =
                stagingDirectory / (prefix + ".zst");
            const auto compressedAvailable = EnsureNewPath(
                compressedPath,
                PackageErrorCode::PublicationFailed,
                "A private package staging path already exists.");
            if (!compressedAvailable) {
                return PackageResult<PackageWriteResult>::Failure(
                    compressedAvailable.error());
            }
            temporaryFiles.Add(compressedPath);
            std::ifstream compressionInput(
                snapshotPath,
                std::ios::binary);
            std::ofstream compressionOutput(
                compressedPath,
                std::ios::binary | std::ios::trunc);
            if (!compressionInput || !compressionOutput) {
                return PackageResult<PackageWriteResult>::Failure(
                    IoError("Opening a compression staging stream failed."));
            }
            const auto compression = compression_.CompressStream(
                compressionInput,
                compressionOutput,
                3);
            compressionInput.close();
            compressionOutput.close();
            if (!compression || !compressionOutput ||
                compression.value().inputSize != entry.size) {
                return PackageResult<PackageWriteResult>::Failure(
                    compression
                        ? IoError(
                            "A staged source changed during compression.")
                        : compression.error());
            }
            if (compression.value().outputSize < entry.size) {
                storedPath = compressedPath;
                storedSize = compression.value().outputSize;
                algorithm = CompressionAlgorithm::Zstandard;
                hasCompressedEntry = true;
                const auto removed =
                    RemoveTemporaryFile(snapshotPath);
                if (!removed) {
                    return PackageResult<PackageWriteResult>::Failure(
                        removed.error());
                }
            } else {
                const auto removed =
                    RemoveTemporaryFile(compressedPath);
                if (!removed) {
                    return PackageResult<PackageWriteResult>::Failure(
                        removed.error());
                }
            }
        }

        ManifestRecord record;
        record.path = entry.packagePath;
        record.compression = algorithm;
        record.plaintextSize = entry.size;
        record.storedSize = storedSize;
        record.payloadOffset = payloadSize;
        record.plaintextSha256 = hash.value().digest;
        record.nonce = entry.nonce;
        const auto nextPayloadSize =
            CheckedAdd(payloadSize, storedSize);
        if (!nextPayloadSize) {
            return PackageResult<PackageWriteResult>::Failure(
                nextPayloadSize.error());
        }
        payloadSize = nextPayloadSize.value();
        staged.push_back({storedPath, std::move(record)});
    }

    PackageManifest manifest;
    manifest.records.reserve(staged.size());
    for (const auto& entry : staged) {
        manifest.records.push_back(entry.record);
    }
    auto manifestPlaintext = SerializeManifest(manifest);
    if (!manifestPlaintext) {
        return PackageResult<PackageWriteResult>::Failure(
            manifestPlaintext.error());
    }
    if (manifestPlaintext.value().size() >
        request.limits.maximumManifestSize) {
        return PackageResult<PackageWriteResult>::Failure(
            WriterError(
                PackageErrorCode::LimitExceeded,
                "The package manifest exceeds the configured limit."));
    }

    PackageHeader header;
    header.entryCount =
        static_cast<std::uint32_t>(staged.size());
    header.manifestCiphertextSize =
        manifestPlaintext.value().size();
    const auto manifestEnd = CheckedAdd(
        kPackageHeaderSize,
        header.manifestCiphertextSize);
    const auto alignedEnd = manifestEnd
        ? CheckedAdd(manifestEnd.value(), 15U)
        : PackageResult<std::uint64_t>::Failure(
            manifestEnd.error());
    if (!manifestEnd || !alignedEnd) {
        return PackageResult<PackageWriteResult>::Failure(
            manifestEnd
                ? alignedEnd.error()
                : manifestEnd.error());
    }
    header.payloadOffset =
        alignedEnd.value() & ~std::uint64_t{15};
    header.payloadSize = payloadSize;
    header.packageId = packageId.value();
    header.keyId = request.keyId;
    header.manifestNonce = manifestNonce.value();
    if (hasCompressedEntry) {
        header.flags |= PackageHeaderFlag::HasCompressedEntries;
    }
    const auto archiveSize =
        CheckedAdd(header.payloadOffset, header.payloadSize);
    if (!archiveSize ||
        archiveSize.value() >
            request.limits.maximumArchiveSize) {
        return PackageResult<PackageWriteResult>::Failure(
            WriterError(
                PackageErrorCode::LimitExceeded,
                "The completed package would exceed its configured limit."));
    }

    const auto packageTemporaryPath =
        stagingDirectory / "package.dbpak.tmp";
    const auto packageTemporaryAvailable = EnsureNewPath(
        packageTemporaryPath,
        PackageErrorCode::PublicationFailed,
        "A private package output path already exists.");
    if (!packageTemporaryAvailable) {
        return PackageResult<PackageWriteResult>::Failure(
            packageTemporaryAvailable.error());
    }
    temporaryFiles.Add(packageTemporaryPath);
    std::fstream packageOutput(
        packageTemporaryPath,
        std::ios::in | std::ios::out |
            std::ios::binary | std::ios::trunc);
    if (!packageOutput) {
        return PackageResult<PackageWriteResult>::Failure(
            IoError("Creating the private package output failed."));
    }
    const auto placeholders =
        WriteZeros(packageOutput, header.payloadOffset);
    if (!placeholders) {
        return PackageResult<PackageWriteResult>::Failure(
            placeholders.error());
    }

    PackageKeyDeriver deriver(crypto_);
    for (std::size_t index = 0; index < staged.size(); ++index) {
        auto& entry = staged[index];
        const auto entryKey = deriver.DeriveEntryKey(
            masterKey.value(),
            header.packageId,
            entry.record.path);
        if (!entryKey) {
            return PackageResult<PackageWriteResult>::Failure(
                entryKey.error());
        }
        std::ifstream storedInput(
            entry.storedPath,
            std::ios::binary);
        if (!storedInput) {
            return PackageResult<PackageWriteResult>::Failure(
                IoError("Opening a staged payload failed."));
        }
        const auto encrypted = crypto_.Aes256GcmEncryptStream(
            entryKey.value(),
            entry.record.nonce,
            storedInput,
            packageOutput,
            BuildEntryAdditionalData(
                header.packageId,
                entry.record),
            entry.record.storedSize);
        if (!encrypted ||
            encrypted.value().outputSize !=
                entry.record.storedSize) {
            return PackageResult<PackageWriteResult>::Failure(
                encrypted
                    ? IoError(
                        "An encrypted payload size was inconsistent.")
                    : encrypted.error());
        }
        entry.record.tag = encrypted.value().tag;
        manifest.records[index] = entry.record;
    }

    manifestPlaintext = SerializeManifest(manifest);
    if (!manifestPlaintext ||
        manifestPlaintext.value().size() !=
            header.manifestCiphertextSize) {
        return PackageResult<PackageWriteResult>::Failure(
            manifestPlaintext
                ? WriterError(
                    PackageErrorCode::IntegrityFailed,
                    "The finalized manifest changed its serialized size.")
                : manifestPlaintext.error());
    }
    const auto manifestHash =
        crypto_.Sha256(manifestPlaintext.value());
    if (!manifestHash) {
        return PackageResult<PackageWriteResult>::Failure(
            manifestHash.error());
    }
    header.manifestPlaintextSha256 = manifestHash.value();
    const auto manifestKey = deriver.DeriveManifestKey(
        masterKey.value(),
        header.packageId);
    if (!manifestKey) {
        return PackageResult<PackageWriteResult>::Failure(
            manifestKey.error());
    }
    const auto encryptedManifest = crypto_.Aes256GcmEncrypt(
        manifestKey.value(),
        header.manifestNonce,
        manifestPlaintext.value(),
        BuildManifestAdditionalData(header));
    if (!encryptedManifest ||
        encryptedManifest.value().ciphertext.size() !=
            header.manifestCiphertextSize) {
        return PackageResult<PackageWriteResult>::Failure(
            encryptedManifest
                ? WriterError(
                    PackageErrorCode::IntegrityFailed,
                    "The encrypted manifest size was inconsistent.")
                : encryptedManifest.error());
    }
    header.manifestTag = encryptedManifest.value().tag;

    const auto headerBytes = SerializePackageHeader(header);
    packageOutput.seekp(0);
    packageOutput.write(
        reinterpret_cast<const char*>(headerBytes.data()),
        static_cast<std::streamsize>(headerBytes.size()));
    packageOutput.write(
        reinterpret_cast<const char*>(
            encryptedManifest.value().ciphertext.data()),
        static_cast<std::streamsize>(
            encryptedManifest.value().ciphertext.size()));
    packageOutput.flush();
    packageOutput.close();
    if (!packageOutput) {
        return PackageResult<PackageWriteResult>::Failure(
            IoError("Finalizing the private package output failed."));
    }

    const auto verified = VerifyWrittenPackage(
        packageTemporaryPath,
        header,
        masterKey.value(),
        request.limits,
        crypto_,
        compression_,
        publisher_);
    if (!verified) {
        return PackageResult<PackageWriteResult>::Failure(
            verified.error());
    }
    const auto published =
        publisher_.Publish(packageTemporaryPath, finalPath);
    if (!published) {
        return PackageResult<PackageWriteResult>::Failure(
            published.error());
    }

    return PackageResult<PackageWriteResult>::Success({
        finalPath,
        header.packageId,
        header,
    });
}

} // namespace dbp::package
