#include "dbp/package/LegacyPckReader.h"

#include "dbp/package/ByteCodec.h"
#include "dbp/package/PackagePath.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace dbp::package {

namespace {

constexpr std::size_t legacyFooterSize = 16U;
constexpr std::uint32_t legacyValidityCode = 12345678U;
constexpr std::uint32_t maximumApplicationKind = 1U;

PackageError Error(
    const PackageErrorCode code,
    std::string message,
    const std::optional<std::uint64_t> offset = std::nullopt) {
    return {
        code,
        std::move(message),
        offset,
    };
}

template <typename T>
PackageResult<T> Failure(
    const PackageErrorCode code,
    std::string message,
    const std::optional<std::uint64_t> offset = std::nullopt) {
    return PackageResult<T>::Failure(
        Error(code, std::move(message), offset));
}

PackageResult<std::vector<std::uint8_t>> ReadExtent(
    std::ifstream& input,
    const std::uint64_t offset,
    const std::size_t size) {
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input) {
        return Failure<std::vector<std::uint8_t>>(
            PackageErrorCode::IoFailed,
            "Seeking within the legacy executable failed.",
            offset);
    }

    std::vector<std::uint8_t> bytes(size);
    if (size != 0) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (input.gcount() !=
            static_cast<std::streamsize>(bytes.size())) {
            return Failure<std::vector<std::uint8_t>>(
                PackageErrorCode::IoFailed,
                "Reading the legacy executable extent failed.",
                offset);
        }
    }
    return PackageResult<std::vector<std::uint8_t>>::Success(
        std::move(bytes));
}

bool IsAsciiEqualIgnoreCase(
    const char left,
    const char right) noexcept {
    const auto leftByte = static_cast<unsigned char>(left);
    const auto rightByte = static_cast<unsigned char>(right);
    const auto fold = [](const unsigned char character) {
        if (character >= 'A' && character <= 'Z') {
            return static_cast<unsigned char>(
                character + ('a' - 'A'));
        }
        return character;
    };
    return fold(leftByte) == fold(rightByte);
}

bool IsAsciiEqualIgnoreCase(
    const std::string_view left,
    const std::string_view right) noexcept {
    return left.size() == right.size() &&
        std::equal(
            left.begin(),
            left.end(),
            right.begin(),
            [](const char leftCharacter, const char rightCharacter) {
                return IsAsciiEqualIgnoreCase(
                    leftCharacter,
                    rightCharacter);
            });
}

bool IsMediaPath(const std::string_view path) noexcept {
    constexpr std::string_view prefix = "media/";
    return path.size() >= prefix.size() &&
        IsAsciiEqualIgnoreCase(path.substr(0, prefix.size()), prefix);
}

void DecryptLegacyMedia(
    std::vector<std::uint8_t>& bytes,
    const std::uint32_t key) noexcept {
    if (key == 0 || bytes.empty()) {
        return;
    }

    const auto shift = static_cast<std::uint8_t>(key % 64U);
    const auto span = std::max<std::size_t>(1U, bytes.size() / 1024U);
    for (std::size_t offset = 0; offset < bytes.size(); offset += span) {
        bytes[offset] = static_cast<std::uint8_t>(bytes[offset] - shift);
    }
}

PackageResult<std::vector<LegacyPckEntry>> ParseArchive(
    const std::vector<std::uint8_t>& archive,
    const std::uint32_t key,
    const LegacyPckLimits& limits) {
    ByteReader reader(archive);
    std::vector<LegacyPckEntry> entries;
    std::vector<std::string> paths;
    std::uint64_t totalPayloadBytes = 0;
    bool foundTerminator = false;

    while (reader.Remaining() != 0) {
        const auto recordOffset =
            static_cast<std::uint64_t>(reader.Position());
        const auto pathSize = reader.ReadUInt32();
        if (!pathSize) {
            return Failure<std::vector<LegacyPckEntry>>(
                PackageErrorCode::InvalidFormat,
                "The legacy PCK record header is truncated.",
                recordOffset);
        }
        if (pathSize.value() == 0) {
            foundTerminator = true;
            if (reader.Remaining() != 0) {
                return Failure<std::vector<LegacyPckEntry>>(
                    PackageErrorCode::InvalidFormat,
                    "Bytes follow the legacy PCK terminator.",
                    reader.Position());
            }
            break;
        }
        if (entries.size() >= limits.maximumEntryCount) {
            return Failure<std::vector<LegacyPckEntry>>(
                PackageErrorCode::LimitExceeded,
                "The legacy PCK entry-count limit was exceeded.",
                recordOffset);
        }
        if (pathSize.value() > limits.maximumPathBytes) {
            return Failure<std::vector<LegacyPckEntry>>(
                PackageErrorCode::LimitExceeded,
                "A legacy PCK path exceeds the configured limit.",
                recordOffset);
        }
        if (pathSize.value() > reader.Remaining()) {
            return Failure<std::vector<LegacyPckEntry>>(
                PackageErrorCode::InvalidFormat,
                "A legacy PCK filename extends beyond the archive.",
                recordOffset);
        }

        const auto pathBytes = reader.ReadBytes(pathSize.value());
        if (!pathBytes) {
            return PackageResult<std::vector<LegacyPckEntry>>::Failure(
                pathBytes.error());
        }
        if (std::find(
                pathBytes.value().begin(),
                pathBytes.value().end(),
                std::uint8_t{0}) != pathBytes.value().end()) {
            return Failure<std::vector<LegacyPckEntry>>(
                PackageErrorCode::InvalidFormat,
                "A legacy PCK filename contains an embedded NUL.",
                recordOffset);
        }
        const std::string sourcePath(
            pathBytes.value().begin(),
            pathBytes.value().end());
        const auto normalizedPath =
            NormalizePackageInputPath(sourcePath);
        if (!normalizedPath) {
            return PackageResult<std::vector<LegacyPckEntry>>::Failure(
                normalizedPath.error());
        }
        if (entries.empty() &&
            IsAsciiEqualIgnoreCase(
                normalizedPath.value(),
                "compress.dll")) {
            return Failure<std::vector<LegacyPckEntry>>(
                PackageErrorCode::UnsupportedAlgorithm,
                "Legacy plugin-compressed PCK archives are not executed.",
                recordOffset);
        }

        const auto dataSizeOffset =
            static_cast<std::uint64_t>(reader.Position());
        const auto dataSize = reader.ReadUInt32();
        if (!dataSize) {
            return Failure<std::vector<LegacyPckEntry>>(
                PackageErrorCode::InvalidFormat,
                "The legacy PCK payload length is truncated.",
                dataSizeOffset);
        }
        if (dataSize.value() > limits.maximumEntryBytes) {
            return Failure<std::vector<LegacyPckEntry>>(
                PackageErrorCode::LimitExceeded,
                "A legacy PCK payload exceeds the configured entry limit.",
                dataSizeOffset);
        }
        const auto nextTotal =
            CheckedAdd(totalPayloadBytes, dataSize.value());
        if (!nextTotal ||
            nextTotal.value() > limits.maximumTotalPayloadBytes) {
            return Failure<std::vector<LegacyPckEntry>>(
                PackageErrorCode::LimitExceeded,
                "The legacy PCK total payload limit was exceeded.",
                dataSizeOffset);
        }
        if (dataSize.value() > reader.Remaining()) {
            return Failure<std::vector<LegacyPckEntry>>(
                PackageErrorCode::InvalidFormat,
                "A legacy PCK payload extends beyond the archive.",
                dataSizeOffset);
        }

        auto payload = reader.ReadBytes(dataSize.value());
        if (!payload) {
            return PackageResult<std::vector<LegacyPckEntry>>::Failure(
                payload.error());
        }
        if (IsMediaPath(normalizedPath.value())) {
            DecryptLegacyMedia(payload.value(), key);
        }
        totalPayloadBytes = nextTotal.value();
        paths.push_back(normalizedPath.value());
        entries.push_back({
            normalizedPath.value(),
            std::make_shared<const std::vector<std::uint8_t>>(
                std::move(payload.value())),
        });
    }

    if (!foundTerminator) {
        return Failure<std::vector<LegacyPckEntry>>(
            PackageErrorCode::InvalidFormat,
            "The legacy PCK terminator is missing.",
            reader.Position());
    }
    const auto validatedPaths = ValidateAndSortPackagePaths(paths);
    if (!validatedPaths) {
        return PackageResult<std::vector<LegacyPckEntry>>::Failure(
            validatedPaths.error());
    }
    std::sort(
        entries.begin(),
        entries.end(),
        [](const LegacyPckEntry& left, const LegacyPckEntry& right) {
            return left.path < right.path;
        });
    return PackageResult<std::vector<LegacyPckEntry>>::Success(
        std::move(entries));
}

} // namespace

LegacyPckReader::LegacyPckReader(
    std::vector<LegacyPckEntry> entries,
    const std::uint32_t applicationKind) noexcept
    : entries_(std::move(entries)),
      applicationKind_(applicationKind) {}

PackageResult<std::unique_ptr<LegacyPckReader>>
LegacyPckReader::OpenExecutable(
    const std::filesystem::path& executablePath,
    const LegacyPckLimits& limits) {
    std::error_code statusError;
    const auto status =
        std::filesystem::symlink_status(executablePath, statusError);
    if (statusError || !std::filesystem::is_regular_file(status)) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::IoFailed,
            "The legacy package source is not a regular file.");
    }

    const auto imageSize =
        std::filesystem::file_size(executablePath, statusError);
    if (statusError) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::IoFailed,
            "Reading the legacy executable size failed.");
    }
    if (imageSize < legacyFooterSize) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::InvalidFormat,
            "The legacy executable footer is truncated.");
    }
    if (imageSize > limits.maximumImageBytes) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::LimitExceeded,
            "The legacy executable exceeds the configured image limit.");
    }

    std::ifstream input(executablePath, std::ios::binary);
    if (!input) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::IoFailed,
            "Opening the legacy executable failed.");
    }
    const auto footerOffset = imageSize - legacyFooterSize;
    const auto footer = ReadExtent(input, footerOffset, legacyFooterSize);
    if (!footer) {
        return PackageResult<std::unique_ptr<LegacyPckReader>>::Failure(
            footer.error());
    }
    ByteReader footerReader(footer.value());
    const auto key = footerReader.ReadUInt32();
    const auto validity = footerReader.ReadUInt32();
    const auto applicationKind = footerReader.ReadUInt32();
    const auto executableSize = footerReader.ReadUInt32();
    if (!key || !validity || !applicationKind || !executableSize) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::InvalidFormat,
            "The legacy executable footer is malformed.",
            footerOffset);
    }
    if (validity.value() != legacyValidityCode) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::InvalidFormat,
            "The legacy executable validity marker is invalid.",
            footerOffset + sizeof(std::uint32_t));
    }
    if (applicationKind.value() > maximumApplicationKind) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::InvalidFormat,
            "The legacy executable application kind is invalid.",
            footerOffset + 2U * sizeof(std::uint32_t));
    }
    if (executableSize.value() > footerOffset) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::InvalidFormat,
            "The legacy executable size extends into its package footer.",
            footerOffset + 3U * sizeof(std::uint32_t));
    }

    const auto archiveSize = footerOffset - executableSize.value();
    if (archiveSize < sizeof(std::uint32_t)) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::InvalidFormat,
            "The legacy PCK terminator is missing.",
            executableSize.value());
    }
    if (archiveSize > limits.maximumArchiveBytes ||
        archiveSize > std::numeric_limits<std::size_t>::max()) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::LimitExceeded,
            "The legacy PCK archive exceeds the configured limit.",
            executableSize.value());
    }
    const auto archive = ReadExtent(
        input,
        executableSize.value(),
        static_cast<std::size_t>(archiveSize));
    if (!archive) {
        return PackageResult<std::unique_ptr<LegacyPckReader>>::Failure(
            archive.error());
    }
    auto entries = ParseArchive(archive.value(), key.value(), limits);
    if (!entries) {
        return PackageResult<std::unique_ptr<LegacyPckReader>>::Failure(
            entries.error());
    }
    return PackageResult<std::unique_ptr<LegacyPckReader>>::Success(
        std::unique_ptr<LegacyPckReader>(
            new LegacyPckReader(
                std::move(entries.value()),
                applicationKind.value())));
}

PackageResult<std::unique_ptr<LegacyPckReader>>
LegacyPckReader::OpenPckFile(
    const std::filesystem::path& pckPath,
    const LegacyPckLimits& limits) {
    std::error_code statusError;
    const auto status =
        std::filesystem::symlink_status(pckPath, statusError);
    if (statusError ||
        !std::filesystem::is_regular_file(status) ||
        std::filesystem::is_symlink(status)) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::IoFailed,
            "The legacy sidecar PCK is not a safe regular file.");
    }
    const auto fileSize =
        std::filesystem::file_size(pckPath, statusError);
    if (statusError) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::IoFailed,
            "Reading the legacy sidecar PCK size failed.");
    }
    if (fileSize < sizeof(std::uint32_t)) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::InvalidFormat,
            "The legacy sidecar PCK terminator is missing.");
    }
    if (fileSize > limits.maximumArchiveBytes ||
        fileSize > (std::numeric_limits<std::size_t>::max)()) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::LimitExceeded,
            "The legacy sidecar PCK exceeds the configured limit.");
    }
    std::ifstream input(pckPath, std::ios::binary);
    if (!input) {
        return Failure<std::unique_ptr<LegacyPckReader>>(
            PackageErrorCode::IoFailed,
            "Opening the legacy sidecar PCK failed.");
    }
    const auto archive =
        ReadExtent(input, 0, static_cast<std::size_t>(fileSize));
    if (!archive) {
        return PackageResult<std::unique_ptr<LegacyPckReader>>::Failure(
            archive.error());
    }
    auto entries = ParseArchive(archive.value(), 0, limits);
    if (!entries) {
        return PackageResult<std::unique_ptr<LegacyPckReader>>::Failure(
            entries.error());
    }
    return PackageResult<std::unique_ptr<LegacyPckReader>>::Success(
        std::unique_ptr<LegacyPckReader>(
            new LegacyPckReader(std::move(entries.value()), 0)));
}

const LegacyPckEntry* LegacyPckReader::FindEntry(
    const std::string_view canonicalPath) const noexcept {
    const auto found = std::find_if(
        entries_.begin(),
        entries_.end(),
        [canonicalPath](const LegacyPckEntry& entry) {
            return IsAsciiEqualIgnoreCase(entry.path, canonicalPath);
        });
    return found == entries_.end() ? nullptr : &*found;
}

} // namespace dbp::package
