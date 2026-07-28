#include "dbp/package/PackageFormat.h"

#include "dbp/package/ByteCodec.h"
#include "dbp/package/PackagePath.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

constexpr std::array<std::uint8_t, 8> packageMagic{
    'D', 'B', 'P', 'P', 'A', 'K', '2', 0,
};
constexpr std::array<std::uint8_t, 8> manifestMagic{
    'D', 'B', 'P', 'M', 'A', 'N', '2', 0,
};
constexpr std::array<std::uint8_t, 16> entryAadPrefix{
    'D', 'B', 'P', '-', 'P', 'A', 'K', '-', 'v', '2', '/', 'e', 'n', 't', 'r', 'y',
};
constexpr std::uint32_t knownHeaderFlags =
    static_cast<std::uint32_t>(PackageHeaderFlag::ManifestEncrypted) |
    static_cast<std::uint32_t>(PackageHeaderFlag::PayloadsEncrypted) |
    static_cast<std::uint32_t>(PackageHeaderFlag::HasCompressedEntries);
constexpr std::uint32_t requiredHeaderFlags =
    static_cast<std::uint32_t>(PackageHeaderFlag::ManifestEncrypted) |
    static_cast<std::uint32_t>(PackageHeaderFlag::PayloadsEncrypted);
constexpr std::uint32_t knownEntryFlags = 1U;

PackageError FormatError(
    PackageErrorCode code,
    std::string message,
    const std::uint64_t offset = 0) {
    return {
        code,
        std::move(message),
        offset,
    };
}

template <std::size_t Size>
void WriteArray(
    ByteWriter& writer,
    const std::array<std::uint8_t, Size>& value) {
    writer.WriteBytes(value.data(), value.size());
}

template <std::size_t Size>
PackageResult<std::array<std::uint8_t, Size>> ReadArray(
    ByteReader& reader) {
    const auto bytes = reader.ReadBytes(Size);
    if (!bytes) {
        return PackageResult<std::array<std::uint8_t, Size>>::Failure(
            bytes.error());
    }
    std::array<std::uint8_t, Size> result{};
    std::copy(bytes.value().begin(), bytes.value().end(), result.begin());
    return PackageResult<std::array<std::uint8_t, Size>>::Success(result);
}

template <std::size_t Size>
bool IsZero(const std::array<std::uint8_t, Size>& value) {
    return std::all_of(
        value.begin(),
        value.end(),
        [](const std::uint8_t byte) { return byte == 0; });
}

bool HasFlag(
    const std::uint32_t flags,
    const PackageHeaderFlag flag) {
    return (flags & static_cast<std::uint32_t>(flag)) != 0;
}

PackageResult<std::vector<ManifestRecord>> CanonicalRecords(
    const PackageManifest& manifest,
    const PackageLimits& limits) {
    if (manifest.records.size() > limits.maximumEntries) {
        return PackageResult<std::vector<ManifestRecord>>::Failure(
            FormatError(
                PackageErrorCode::LimitExceeded,
                "Manifest entry count exceeds the configured limit."));
    }

    std::vector<ManifestRecord> records = manifest.records;
    std::vector<std::string> normalizedPaths;
    normalizedPaths.reserve(records.size());
    for (auto& record : records) {
        const auto normalized = NormalizePackageInputPath(record.path);
        if (!normalized) {
            return PackageResult<std::vector<ManifestRecord>>::Failure(
                normalized.error());
        }
        if (normalized.value().size() > limits.maximumPathBytes) {
            return PackageResult<std::vector<ManifestRecord>>::Failure(
                FormatError(
                    PackageErrorCode::LimitExceeded,
                    "Manifest path exceeds the configured limit."));
        }
        record.path = normalized.value();
        normalizedPaths.push_back(record.path);

        if ((record.flags & ~knownEntryFlags) != 0) {
            return PackageResult<std::vector<ManifestRecord>>::Failure(
                FormatError(
                    PackageErrorCode::InvalidFormat,
                    "Manifest record contains unknown flags."));
        }
        if (record.compression != CompressionAlgorithm::None &&
            record.compression != CompressionAlgorithm::Zstandard) {
            return PackageResult<std::vector<ManifestRecord>>::Failure(
                FormatError(
                    PackageErrorCode::UnsupportedAlgorithm,
                    "Manifest record uses an unsupported compression algorithm."));
        }
        if (record.encryption != EncryptionAlgorithm::Aes256Gcm) {
            return PackageResult<std::vector<ManifestRecord>>::Failure(
                FormatError(
                    PackageErrorCode::UnsupportedAlgorithm,
                    "Manifest record uses an unsupported encryption algorithm."));
        }
        if (record.plaintextSize > limits.maximumEntryPlaintextSize) {
            return PackageResult<std::vector<ManifestRecord>>::Failure(
                FormatError(
                    PackageErrorCode::LimitExceeded,
                    "Manifest entry exceeds the plaintext size limit."));
        }
    }

    const auto validatedPaths =
        ValidateAndSortPackagePaths(normalizedPaths);
    if (!validatedPaths) {
        return PackageResult<std::vector<ManifestRecord>>::Failure(
            validatedPaths.error());
    }

    std::sort(
        records.begin(),
        records.end(),
        [](const ManifestRecord& left, const ManifestRecord& right) {
            return left.path < right.path;
        });

    std::uint64_t expectedPayloadOffset = 0;
    std::uint64_t totalPlaintext = 0;
    for (const auto& record : records) {
        if (record.payloadOffset != expectedPayloadOffset) {
            return PackageResult<std::vector<ManifestRecord>>::Failure(
                FormatError(
                    PackageErrorCode::InvalidFormat,
                    "Manifest payload extents are not contiguous."));
        }
        const auto nextPayloadOffset =
            CheckedAdd(expectedPayloadOffset, record.storedSize);
        if (!nextPayloadOffset) {
            return PackageResult<std::vector<ManifestRecord>>::Failure(
                nextPayloadOffset.error());
        }
        expectedPayloadOffset = nextPayloadOffset.value();

        const auto nextPlaintext =
            CheckedAdd(totalPlaintext, record.plaintextSize);
        if (!nextPlaintext ||
            nextPlaintext.value() > limits.maximumTotalPlaintextSize) {
            return PackageResult<std::vector<ManifestRecord>>::Failure(
                FormatError(
                    PackageErrorCode::LimitExceeded,
                    "Manifest total plaintext exceeds the configured limit."));
        }
        totalPlaintext = nextPlaintext.value();
    }

    return PackageResult<std::vector<ManifestRecord>>::Success(
        std::move(records));
}

} // namespace

std::vector<std::uint8_t> SerializePackageHeader(
    const PackageHeader& header) {
    ByteWriter writer;
    WriteArray(writer, packageMagic);
    writer.WriteUInt16(header.majorVersion);
    writer.WriteUInt16(header.minorVersion);
    writer.WriteUInt32(kPackageHeaderSize);
    writer.WriteUInt32(header.flags);
    writer.WriteUInt32(header.entryCount);
    writer.WriteUInt64(header.manifestOffset);
    writer.WriteUInt64(header.manifestCiphertextSize);
    writer.WriteUInt64(header.payloadOffset);
    writer.WriteUInt64(header.payloadSize);
    WriteArray(writer, header.packageId);
    WriteArray(writer, header.keyId);
    WriteArray(writer, header.manifestNonce);
    WriteArray(writer, header.manifestTag);
    WriteArray(writer, header.manifestPlaintextSha256);
    constexpr std::array<std::uint8_t, 12> reserved{};
    WriteArray(writer, reserved);
    return writer.Bytes();
}

PackageResult<PackageHeader> ParsePackageHeader(
    const std::vector<std::uint8_t>& bytes,
    const std::uint64_t fileSize,
    const PackageLimits& limits) {
    if (bytes.size() != kPackageHeaderSize ||
        fileSize < kPackageHeaderSize) {
        return PackageResult<PackageHeader>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "DBPAK header has an invalid size."));
    }
    if (fileSize > limits.maximumArchiveSize) {
        return PackageResult<PackageHeader>::Failure(FormatError(
            PackageErrorCode::LimitExceeded,
            "DBPAK archive exceeds the configured size limit."));
    }

    ByteReader reader(bytes);
    const auto magic = ReadArray<8>(reader);
    const auto major = reader.ReadUInt16();
    const auto minor = reader.ReadUInt16();
    const auto headerSize = reader.ReadUInt32();
    const auto flags = reader.ReadUInt32();
    const auto entryCount = reader.ReadUInt32();
    const auto manifestOffset = reader.ReadUInt64();
    const auto manifestSize = reader.ReadUInt64();
    const auto payloadOffset = reader.ReadUInt64();
    const auto payloadSize = reader.ReadUInt64();
    const auto packageId = ReadArray<16>(reader);
    const auto keyId = ReadArray<16>(reader);
    const auto manifestNonce = ReadArray<12>(reader);
    const auto manifestTag = ReadArray<16>(reader);
    const auto manifestHash = ReadArray<32>(reader);
    const auto reserved = ReadArray<12>(reader);

    if (!magic || !major || !minor || !headerSize || !flags ||
        !entryCount || !manifestOffset || !manifestSize ||
        !payloadOffset || !payloadSize || !packageId || !keyId ||
        !manifestNonce || !manifestTag || !manifestHash || !reserved) {
        return PackageResult<PackageHeader>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "DBPAK header is truncated."));
    }
    if (magic.value() != packageMagic) {
        return PackageResult<PackageHeader>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "DBPAK header magic is invalid."));
    }
    if (major.value() != 2) {
        return PackageResult<PackageHeader>::Failure(FormatError(
            PackageErrorCode::UnsupportedVersion,
            "DBPAK major version is unsupported."));
    }
    if (headerSize.value() != kPackageHeaderSize) {
        return PackageResult<PackageHeader>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "DBPAK persisted header size is invalid."));
    }
    if ((flags.value() & ~knownHeaderFlags) != 0 ||
        (flags.value() & requiredHeaderFlags) != requiredHeaderFlags) {
        return PackageResult<PackageHeader>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "DBPAK header flags are unsupported or incomplete."));
    }
    if (entryCount.value() > limits.maximumEntries) {
        return PackageResult<PackageHeader>::Failure(FormatError(
            PackageErrorCode::LimitExceeded,
            "DBPAK entry count exceeds the configured limit."));
    }
    if (manifestOffset.value() != kPackageHeaderSize ||
        manifestSize.value() < kManifestHeaderSize ||
        manifestSize.value() > limits.maximumManifestSize ||
        payloadOffset.value() % 16U != 0) {
        return PackageResult<PackageHeader>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "DBPAK manifest or payload layout is invalid."));
    }

    const auto manifestEnd =
        CheckedAdd(manifestOffset.value(), manifestSize.value());
    const auto alignedManifestEnd = manifestEnd
        ? CheckedAdd(manifestEnd.value(), 15U)
        : PackageResult<std::uint64_t>::Failure(manifestEnd.error());
    const auto payloadEnd =
        CheckedAdd(payloadOffset.value(), payloadSize.value());
    if (!manifestEnd || !alignedManifestEnd || !payloadEnd ||
        (alignedManifestEnd.value() & ~std::uint64_t{15}) !=
            payloadOffset.value() ||
        payloadEnd.value() != fileSize) {
        return PackageResult<PackageHeader>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "DBPAK declared extents do not match the file."));
    }
    if (!IsZero(reserved.value())) {
        return PackageResult<PackageHeader>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "DBPAK header reserved bytes must be zero."));
    }

    PackageHeader result;
    result.majorVersion = major.value();
    result.minorVersion = minor.value();
    result.flags = flags.value();
    result.entryCount = entryCount.value();
    result.manifestOffset = manifestOffset.value();
    result.manifestCiphertextSize = manifestSize.value();
    result.payloadOffset = payloadOffset.value();
    result.payloadSize = payloadSize.value();
    result.packageId = packageId.value();
    result.keyId = keyId.value();
    result.manifestNonce = manifestNonce.value();
    result.manifestTag = manifestTag.value();
    result.manifestPlaintextSha256 = manifestHash.value();
    return PackageResult<PackageHeader>::Success(std::move(result));
}

PackageResult<std::vector<std::uint8_t>> SerializeManifest(
    const PackageManifest& manifest) {
    const PackageLimits limits;
    const auto canonical = CanonicalRecords(manifest, limits);
    if (!canonical) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            canonical.error());
    }

    std::uint64_t stringTableSize = 0;
    for (const auto& record : canonical.value()) {
        const auto nextSize =
            CheckedAdd(stringTableSize, record.path.size());
        if (!nextSize) {
            return PackageResult<std::vector<std::uint8_t>>::Failure(
                nextSize.error());
        }
        stringTableSize = nextSize.value();
    }
    const auto recordsSize = CheckedMultiply(
        canonical.value().size(),
        kManifestRecordSize);
    if (!recordsSize) {
        return PackageResult<std::vector<std::uint8_t>>::Failure(
            recordsSize.error());
    }

    ByteWriter writer;
    WriteArray(writer, manifestMagic);
    writer.WriteUInt16(2);
    writer.WriteUInt16(0);
    writer.WriteUInt32(
        static_cast<std::uint32_t>(canonical.value().size()));
    writer.WriteUInt64(recordsSize.value());
    writer.WriteUInt64(stringTableSize);

    std::uint64_t pathOffset = 0;
    for (const auto& record : canonical.value()) {
        writer.WriteUInt64(pathOffset);
        writer.WriteUInt32(
            static_cast<std::uint32_t>(record.path.size()));
        writer.WriteUInt32(record.flags);
        writer.WriteUInt16(
            static_cast<std::uint16_t>(record.compression));
        writer.WriteUInt16(
            static_cast<std::uint16_t>(record.encryption));
        writer.WriteUInt32(0);
        writer.WriteUInt64(record.plaintextSize);
        writer.WriteUInt64(record.storedSize);
        writer.WriteUInt64(record.payloadOffset);
        WriteArray(writer, record.plaintextSha256);
        WriteArray(writer, record.nonce);
        WriteArray(writer, record.tag);
        writer.WriteUInt32(0);
        pathOffset += record.path.size();
    }
    for (const auto& record : canonical.value()) {
        writer.WriteBytes(
            reinterpret_cast<const std::uint8_t*>(record.path.data()),
            record.path.size());
    }
    return PackageResult<std::vector<std::uint8_t>>::Success(
        writer.Bytes());
}

PackageResult<PackageManifest> ParseManifest(
    const std::vector<std::uint8_t>& plaintext,
    const PackageHeader& header,
    const PackageLimits& limits) {
    if (plaintext.size() < kManifestHeaderSize ||
        plaintext.size() > limits.maximumManifestSize) {
        return PackageResult<PackageManifest>::Failure(FormatError(
            PackageErrorCode::LimitExceeded,
            "Manifest size is outside the configured limits."));
    }

    ByteReader reader(plaintext);
    const auto magic = ReadArray<8>(reader);
    const auto major = reader.ReadUInt16();
    const auto minor = reader.ReadUInt16();
    const auto recordCount = reader.ReadUInt32();
    const auto recordsSize = reader.ReadUInt64();
    const auto stringTableSize = reader.ReadUInt64();
    if (!magic || !major || !minor || !recordCount ||
        !recordsSize || !stringTableSize) {
        return PackageResult<PackageManifest>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "Manifest header is truncated."));
    }
    if (magic.value() != manifestMagic || major.value() != 2) {
        return PackageResult<PackageManifest>::Failure(FormatError(
            major.value() == 2
                ? PackageErrorCode::InvalidFormat
                : PackageErrorCode::UnsupportedVersion,
            "Manifest magic or version is unsupported."));
    }
    if (recordCount.value() != header.entryCount ||
        recordCount.value() > limits.maximumEntries) {
        return PackageResult<PackageManifest>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "Manifest record count does not match the package header."));
    }
    const auto expectedRecordsSize =
        CheckedMultiply(recordCount.value(), kManifestRecordSize);
    if (!expectedRecordsSize ||
        recordsSize.value() != expectedRecordsSize.value()) {
        return PackageResult<PackageManifest>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "Manifest record-array size is invalid."));
    }
    const auto bodySize =
        CheckedAdd(recordsSize.value(), stringTableSize.value());
    const auto totalSize = bodySize
        ? CheckedAdd(kManifestHeaderSize, bodySize.value())
        : PackageResult<std::uint64_t>::Failure(bodySize.error());
    if (!totalSize || totalSize.value() != plaintext.size()) {
        return PackageResult<PackageManifest>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "Manifest declared size does not match its plaintext."));
    }

    struct RawRecord {
        std::uint64_t pathOffset;
        std::uint32_t pathSize;
        ManifestRecord record;
    };
    std::vector<RawRecord> rawRecords;
    rawRecords.reserve(recordCount.value());
    for (std::uint32_t index = 0; index < recordCount.value(); ++index) {
        const auto pathOffset = reader.ReadUInt64();
        const auto pathSize = reader.ReadUInt32();
        const auto flags = reader.ReadUInt32();
        const auto compression = reader.ReadUInt16();
        const auto encryption = reader.ReadUInt16();
        const auto reservedOne = reader.ReadUInt32();
        const auto plaintextSize = reader.ReadUInt64();
        const auto storedSize = reader.ReadUInt64();
        const auto payloadOffset = reader.ReadUInt64();
        const auto plaintextHash = ReadArray<32>(reader);
        const auto nonce = ReadArray<12>(reader);
        const auto tag = ReadArray<16>(reader);
        const auto reservedTwo = reader.ReadUInt32();
        if (!pathOffset || !pathSize || !flags || !compression ||
            !encryption || !reservedOne || !plaintextSize ||
            !storedSize || !payloadOffset || !plaintextHash ||
            !nonce || !tag || !reservedTwo) {
            return PackageResult<PackageManifest>::Failure(FormatError(
                PackageErrorCode::InvalidFormat,
                "Manifest record is truncated."));
        }
        if (reservedOne.value() != 0 || reservedTwo.value() != 0 ||
            (flags.value() & ~knownEntryFlags) != 0) {
            return PackageResult<PackageManifest>::Failure(FormatError(
                PackageErrorCode::InvalidFormat,
                "Manifest record contains unknown flags or reserved data."));
        }
        if (compression.value() >
                static_cast<std::uint16_t>(CompressionAlgorithm::Zstandard) ||
            encryption.value() !=
                static_cast<std::uint16_t>(EncryptionAlgorithm::Aes256Gcm)) {
            return PackageResult<PackageManifest>::Failure(FormatError(
                PackageErrorCode::UnsupportedAlgorithm,
                "Manifest record uses an unsupported algorithm."));
        }
        if (pathSize.value() == 0 ||
            pathSize.value() > limits.maximumPathBytes ||
            plaintextSize.value() > limits.maximumEntryPlaintextSize) {
            return PackageResult<PackageManifest>::Failure(FormatError(
                PackageErrorCode::LimitExceeded,
                "Manifest record exceeds a configured limit."));
        }

        ManifestRecord record;
        record.flags = flags.value();
        record.compression =
            static_cast<CompressionAlgorithm>(compression.value());
        record.encryption =
            static_cast<EncryptionAlgorithm>(encryption.value());
        record.plaintextSize = plaintextSize.value();
        record.storedSize = storedSize.value();
        record.payloadOffset = payloadOffset.value();
        record.plaintextSha256 = plaintextHash.value();
        record.nonce = nonce.value();
        record.tag = tag.value();
        rawRecords.push_back({
            pathOffset.value(),
            pathSize.value(),
            std::move(record),
        });
    }

    const auto stringTableBytes =
        reader.ReadBytes(static_cast<std::size_t>(stringTableSize.value()));
    if (!stringTableBytes || reader.Remaining() != 0) {
        return PackageResult<PackageManifest>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "Manifest string table is truncated."));
    }

    PackageManifest result;
    result.records.reserve(rawRecords.size());
    std::vector<std::string> paths;
    paths.reserve(rawRecords.size());
    std::uint64_t expectedPathOffset = 0;
    std::uint64_t expectedPayloadOffset = 0;
    std::uint64_t totalPlaintext = 0;
    bool hasCompressedEntry = false;
    for (auto& raw : rawRecords) {
        if (raw.pathOffset != expectedPathOffset) {
            return PackageResult<PackageManifest>::Failure(FormatError(
                PackageErrorCode::InvalidFormat,
                "Manifest string-table extents are not contiguous."));
        }
        const auto pathEnd = CheckedAdd(raw.pathOffset, raw.pathSize);
        if (!pathEnd || pathEnd.value() > stringTableBytes.value().size()) {
            return PackageResult<PackageManifest>::Failure(FormatError(
                PackageErrorCode::InvalidFormat,
                "Manifest path extent is outside the string table."));
        }
        raw.record.path.assign(
            reinterpret_cast<const char*>(
                stringTableBytes.value().data() + raw.pathOffset),
            raw.pathSize);
        const auto validated =
            ValidatePersistedPackagePath(raw.record.path);
        if (!validated) {
            return PackageResult<PackageManifest>::Failure(
                validated.error());
        }
        paths.push_back(raw.record.path);
        expectedPathOffset = pathEnd.value();

        if (raw.record.payloadOffset != expectedPayloadOffset) {
            return PackageResult<PackageManifest>::Failure(FormatError(
                PackageErrorCode::InvalidFormat,
                "Manifest payload extents are not contiguous."));
        }
        const auto payloadEnd =
            CheckedAdd(expectedPayloadOffset, raw.record.storedSize);
        if (!payloadEnd || payloadEnd.value() > header.payloadSize) {
            return PackageResult<PackageManifest>::Failure(FormatError(
                PackageErrorCode::InvalidFormat,
                "Manifest payload extent is outside the payload region."));
        }
        expectedPayloadOffset = payloadEnd.value();

        const auto nextPlaintext =
            CheckedAdd(totalPlaintext, raw.record.plaintextSize);
        if (!nextPlaintext ||
            nextPlaintext.value() > limits.maximumTotalPlaintextSize) {
            return PackageResult<PackageManifest>::Failure(FormatError(
                PackageErrorCode::LimitExceeded,
                "Manifest total plaintext exceeds the configured limit."));
        }
        totalPlaintext = nextPlaintext.value();
        hasCompressedEntry =
            hasCompressedEntry ||
            raw.record.compression == CompressionAlgorithm::Zstandard;
        result.records.push_back(std::move(raw.record));
    }
    if (expectedPathOffset != stringTableBytes.value().size() ||
        expectedPayloadOffset != header.payloadSize) {
        return PackageResult<PackageManifest>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "Manifest does not consume its declared regions exactly."));
    }

    const auto sortedPaths = ValidateAndSortPackagePaths(paths);
    if (!sortedPaths || sortedPaths.value() != paths) {
        return PackageResult<PackageManifest>::Failure(
            sortedPaths
                ? FormatError(
                    PackageErrorCode::InvalidFormat,
                    "Manifest records are not sorted by canonical path.")
                : sortedPaths.error());
    }
    if (hasCompressedEntry !=
        HasFlag(header.flags, PackageHeaderFlag::HasCompressedEntries)) {
        return PackageResult<PackageManifest>::Failure(FormatError(
            PackageErrorCode::InvalidFormat,
            "Manifest compression state does not match the package header."));
    }
    return PackageResult<PackageManifest>::Success(std::move(result));
}

std::vector<std::uint8_t> BuildManifestAdditionalData(
    const PackageHeader& header) {
    auto bytes = SerializePackageHeader(header);
    std::fill(
        bytes.begin() + 100,
        bytes.begin() + 116,
        std::uint8_t{0});
    return bytes;
}

std::vector<std::uint8_t> BuildEntryAdditionalData(
    const PackageId& packageId,
    const ManifestRecord& record) {
    ByteWriter writer;
    WriteArray(writer, entryAadPrefix);
    WriteArray(writer, packageId);
    writer.WriteUInt32(static_cast<std::uint32_t>(record.path.size()));
    writer.WriteBytes(
        reinterpret_cast<const std::uint8_t*>(record.path.data()),
        record.path.size());
    writer.WriteUInt32(record.flags);
    writer.WriteUInt16(
        static_cast<std::uint16_t>(record.compression));
    writer.WriteUInt16(
        static_cast<std::uint16_t>(record.encryption));
    writer.WriteUInt64(record.plaintextSize);
    writer.WriteUInt64(record.storedSize);
    writer.WriteUInt64(record.payloadOffset);
    WriteArray(writer, record.plaintextSha256);
    WriteArray(writer, record.nonce);
    return writer.Bytes();
}

} // namespace dbp::package
