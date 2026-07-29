#include "DBProTools/Publisher/PublisherManifest.h"

#include "dbp/package/PackagePath.h"

#include <nlohmann/json.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dbp::publisher {

namespace {

using json = nlohmann::json;
using package::PackageError;
using package::PackageErrorCode;
using package::PackageResult;

constexpr std::uint64_t kMaximumManifestDocumentSize =
    16ULL * 1024ULL * 1024ULL;
constexpr std::size_t kMaximumJsonDepth = 8U;

class FileHandle {
public:
    explicit FileHandle(const HANDLE handle) noexcept
        : handle_(handle) {}

    ~FileHandle() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    HANDLE get() const noexcept {
        return handle_;
    }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

PackageError Error(
    const PackageErrorCode code,
    std::string message) {
    return {code, std::move(message), std::nullopt};
}

PackageResult<PublisherManifest> Failure(
    const PackageErrorCode code,
    std::string message) {
    return PackageResult<PublisherManifest>::Failure(
        Error(code, std::move(message)));
}

bool HasOnlyKeys(
    const json& object,
    const std::set<std::string, std::less<>>& allowed) {
    if (!object.is_object()) {
        return false;
    }
    for (auto item = object.begin();
         item != object.end();
         ++item) {
        if (allowed.find(item.key()) == allowed.end()) {
            return false;
        }
    }
    return true;
}

PackageResult<std::filesystem::path> ResolvePath(
    const std::filesystem::path& base,
    const std::string& utf8,
    const char* const message) {
    if (utf8.empty() ||
        utf8.find('\0') != std::string::npos) {
        return PackageResult<std::filesystem::path>::Failure(
            Error(PackageErrorCode::InvalidFormat, message));
    }
    std::filesystem::path input;
    try {
        input = std::filesystem::u8path(utf8);
    } catch (const std::exception&) {
        return PackageResult<std::filesystem::path>::Failure(
            Error(PackageErrorCode::InvalidFormat, message));
    }
    std::error_code error;
    const auto combined =
        input.is_absolute() ? input : base / input;
    auto absolute =
        std::filesystem::absolute(combined, error);
    if (error) {
        return PackageResult<std::filesystem::path>::Failure(
            Error(PackageErrorCode::IoFailed, message));
    }
    return PackageResult<std::filesystem::path>::Success(
        absolute.lexically_normal());
}

PackageResult<bool> RequireRegularFile(
    const std::filesystem::path& path,
    const char* const message) {
    std::error_code error;
    const auto status =
        std::filesystem::symlink_status(path, error);
    if (error ||
        !std::filesystem::is_regular_file(status)) {
        return PackageResult<bool>::Failure(
            Error(PackageErrorCode::IoFailed, message));
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<std::vector<std::uint8_t>>
ReadManifestBytes(
    const std::filesystem::path& path) {
    FileHandle handle(CreateFileW(
        path.c_str(),
        GENERIC_READ | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT |
            FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        return PackageResult<
            std::vector<std::uint8_t>>::Failure(
                Error(
                    PackageErrorCode::IoFailed,
                    "Opening the publisher manifest failed."));
    }
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(
            handle.get(),
            &information) ||
        (information.dwFileAttributes &
            (FILE_ATTRIBUTE_DIRECTORY |
             FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
        return PackageResult<
            std::vector<std::uint8_t>>::Failure(
                Error(
                    PackageErrorCode::IoFailed,
                    "The publisher manifest is not a regular file."));
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle.get(), &size) ||
        size.QuadPart < 0) {
        return PackageResult<
            std::vector<std::uint8_t>>::Failure(
                Error(
                    PackageErrorCode::IoFailed,
                    "Reading the publisher manifest size failed."));
    }
    if (static_cast<std::uint64_t>(size.QuadPart) >
        kMaximumManifestDocumentSize) {
        return PackageResult<
            std::vector<std::uint8_t>>::Failure(
                Error(
                    PackageErrorCode::LimitExceeded,
                    "The publisher manifest document is too large."));
    }
    std::vector<std::uint8_t> bytes(
        static_cast<std::size_t>(size.QuadPart));
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto amount = static_cast<DWORD>(
            std::min<std::size_t>(
                bytes.size() - offset,
                MAXDWORD));
        DWORD read = 0;
        if (!ReadFile(
                handle.get(),
                bytes.data() + offset,
                amount,
                &read,
                nullptr) ||
            read == 0) {
            return PackageResult<
                std::vector<std::uint8_t>>::Failure(
                    Error(
                        PackageErrorCode::IoFailed,
                        "Reading the publisher manifest failed."));
        }
        offset += read;
    }
    std::uint8_t extra = 0;
    DWORD extraRead = 0;
    if (!ReadFile(
            handle.get(),
            &extra,
            1,
            &extraRead,
            nullptr) ||
        extraRead != 0) {
        return PackageResult<
            std::vector<std::uint8_t>>::Failure(
                Error(
                    PackageErrorCode::LimitExceeded,
                    "The publisher manifest changed during its bounded read."));
    }
    return PackageResult<
        std::vector<std::uint8_t>>::Success(
            std::move(bytes));
}

PackageResult<bool> ValidateJsonDepth(
    const std::vector<std::uint8_t>& bytes) {
    std::size_t depth = 0;
    bool inString = false;
    bool escaped = false;
    for (const auto byte : bytes) {
        const auto character = static_cast<char>(byte);
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
        } else if (character == '{' ||
                   character == '[') {
            ++depth;
            if (depth > kMaximumJsonDepth) {
                return PackageResult<bool>::Failure(
                    Error(
                        PackageErrorCode::LimitExceeded,
                        "The publisher manifest JSON is nested too deeply."));
            }
        } else if (
            character == '}' ||
            character == ']') {
            if (depth > 0) {
                --depth;
            }
        }
    }
    return PackageResult<bool>::Success(true);
}

PackageResult<json> ParseStrictJson(
    const std::vector<std::uint8_t>& bytes) {
    bool duplicateKey = false;
    std::vector<std::unordered_set<std::string>> objectKeys;
    const auto callback =
        [&](const int depth,
            const json::parse_event_t event,
            json& parsed) {
            const auto index =
                static_cast<std::size_t>(std::max(depth, 0));
            if (objectKeys.size() <= index + 1U) {
                objectKeys.resize(index + 2U);
            }
            if (event == json::parse_event_t::object_start) {
                objectKeys[index].clear();
                objectKeys[index + 1U].clear();
            } else if (event == json::parse_event_t::key) {
                if (!objectKeys[index].insert(
                        parsed.get<std::string>()).second) {
                    duplicateKey = true;
                }
            } else if (
                event == json::parse_event_t::object_end) {
                objectKeys[index].clear();
                objectKeys[index + 1U].clear();
            }
            return true;
        };
    try {
        auto document = json::parse(
            bytes.begin(),
            bytes.end(),
            callback,
            true,
            false);
        if (duplicateKey) {
            return PackageResult<json>::Failure(
                Error(
                    PackageErrorCode::InvalidFormat,
                    "The publisher manifest contains a duplicate key."));
        }
        return PackageResult<json>::Success(
            std::move(document));
    } catch (const json::exception&) {
        return PackageResult<json>::Failure(
            Error(
                PackageErrorCode::InvalidFormat,
                "The publisher manifest is not valid strict JSON."));
    }
}

} // namespace

PackageResult<PublisherManifest> ReadPublisherManifest(
    const std::filesystem::path& manifestPath,
    const package::PackageLimits& limits) {
    const auto manifestBytes =
        ReadManifestBytes(manifestPath);
    if (!manifestBytes) {
        return PackageResult<PublisherManifest>::Failure(
            manifestBytes.error());
    }
    const auto depthValid =
        ValidateJsonDepth(manifestBytes.value());
    if (!depthValid) {
        return PackageResult<PublisherManifest>::Failure(
            depthValid.error());
    }
    const auto parsed =
        ParseStrictJson(manifestBytes.value());
    if (!parsed) {
        return PackageResult<PublisherManifest>::Failure(
            parsed.error());
    }
    const auto& root = parsed.value();
    const std::set<std::string, std::less<>> rootKeys{
        "assets",
        "hostExecutable",
        "mode",
        "outputExecutable",
        "schemaVersion",
    };
    if (!HasOnlyKeys(root, rootKeys) ||
        !root.contains("schemaVersion") ||
        !root.contains("hostExecutable") ||
        !root.contains("outputExecutable") ||
        !root.contains("assets") ||
        !root["schemaVersion"].is_number_unsigned() ||
        !root["hostExecutable"].is_string() ||
        !root["outputExecutable"].is_string() ||
        !root["assets"].is_array()) {
        return Failure(
            PackageErrorCode::InvalidFormat,
            "The publisher manifest root contract is invalid.");
    }
    const auto schemaVersion =
        root["schemaVersion"].get<std::uint64_t>();
    if (schemaVersion != 1U) {
        return Failure(
            PackageErrorCode::InvalidFormat,
            "The publisher manifest schema version is unsupported.");
    }
    if (root["assets"].size() > limits.maximumEntries) {
        return Failure(
            PackageErrorCode::LimitExceeded,
            "The publisher manifest has too many assets.");
    }

    std::error_code absoluteError;
    auto absoluteManifest = std::filesystem::absolute(
        manifestPath,
        absoluteError);
    if (absoluteError) {
        return Failure(
            PackageErrorCode::IoFailed,
            "Resolving the publisher manifest path failed.");
    }
    const auto base = absoluteManifest
        .lexically_normal()
        .parent_path();
    const auto host = ResolvePath(
        base,
        root["hostExecutable"].get<std::string>(),
        "The host executable path is invalid.");
    const auto output = ResolvePath(
        base,
        root["outputExecutable"].get<std::string>(),
        "The output executable path is invalid.");
    if (!host || !output) {
        return PackageResult<PublisherManifest>::Failure(
            !host ? host.error() : output.error());
    }
    const auto hostValid = RequireRegularFile(
        host.value(),
        "The host executable is not a regular file.");
    if (!hostValid) {
        return PackageResult<PublisherManifest>::Failure(
            hostValid.error());
    }

    PublisherManifest result;
    result.schemaVersion = 1U;
    result.hostExecutable = host.value();
    result.outputExecutable = output.value();
    if (root.contains("mode")) {
        if (!root["mode"].is_string()) {
            return Failure(
                PackageErrorCode::InvalidFormat,
                "The publisher mode must be a string.");
        }
        const auto mode =
            root["mode"].get<std::string>();
        if (mode == "application") {
            result.mode =
                package::RuntimeMode::Application;
        } else if (mode == "installer") {
            result.mode =
                package::RuntimeMode::Installer;
        } else {
            return Failure(
                PackageErrorCode::InvalidFormat,
                "The publisher mode is invalid.");
        }
    }

    const std::set<std::string, std::less<>> assetKeys{
        "compress",
        "destination",
        "source",
    };
    std::vector<std::string> destinations;
    std::uint64_t totalSize = 0;
    result.assets.reserve(root["assets"].size());
    destinations.reserve(root["assets"].size());
    for (const auto& asset : root["assets"]) {
        if (!HasOnlyKeys(asset, assetKeys) ||
            !asset.contains("source") ||
            !asset.contains("destination") ||
            !asset["source"].is_string() ||
            !asset["destination"].is_string() ||
            (asset.contains("compress") &&
             !asset["compress"].is_boolean())) {
            return Failure(
                PackageErrorCode::InvalidFormat,
                "A publisher asset contract is invalid.");
        }
        const auto source = ResolvePath(
            base,
            asset["source"].get<std::string>(),
            "A publisher asset source path is invalid.");
        if (!source) {
            return PackageResult<PublisherManifest>::Failure(
                source.error());
        }
        const auto sourceIdentity =
            package::CapturePackageSourceIdentity(
                source.value());
        if (!sourceIdentity) {
            return PackageResult<PublisherManifest>::Failure(
                sourceIdentity.error());
        }
        const auto destination =
            package::NormalizePackageInputPath(
                asset["destination"].get<std::string>());
        if (!destination) {
            return PackageResult<PublisherManifest>::Failure(
                destination.error());
        }
        if (destination.value().size() >
            limits.maximumPathBytes) {
            return Failure(
                PackageErrorCode::LimitExceeded,
                "A publisher asset destination is too long.");
        }
        const auto fileSize = sourceIdentity.value().size;
        if (fileSize >
            limits.maximumEntryPlaintextSize) {
            return Failure(
                PackageErrorCode::LimitExceeded,
                "A publisher asset exceeds its size limit.");
        }
        if (fileSize >
            std::numeric_limits<std::uint64_t>::max() -
                totalSize) {
            return Failure(
                PackageErrorCode::LimitExceeded,
                "Publisher asset sizes overflow.");
        }
        totalSize += fileSize;
        if (totalSize >
            limits.maximumTotalPlaintextSize) {
            return Failure(
                PackageErrorCode::LimitExceeded,
                "Publisher assets exceed their total size limit.");
        }
        destinations.push_back(destination.value());
        result.assets.push_back({
            source.value(),
            destination.value(),
            asset.value("compress", true),
            sourceIdentity.value(),
        });
    }
    const auto pathsValid =
        package::ValidateAndSortPackagePaths(
            destinations);
    if (!pathsValid) {
        return PackageResult<PublisherManifest>::Failure(
            pathsValid.error());
    }
    return PackageResult<PublisherManifest>::Success(
        std::move(result));
}

package::ApplicationPublishRequest
BuildApplicationPublishRequest(
    const PublisherManifest& manifest,
    const package::KeyId& keyId,
    const package::PackageLimits& limits) {
    package::ApplicationPublishRequest request;
    request.hostExecutable = manifest.hostExecutable;
    request.outputExecutable = manifest.outputExecutable;
    request.mode = manifest.mode;
    request.keyId = keyId;
    request.limits = limits;
    request.entries.reserve(manifest.assets.size());
    for (const auto& asset : manifest.assets) {
        request.entries.push_back({
            asset.source,
            asset.destination,
            asset.compress,
            asset.sourceIdentity,
        });
    }
    return request;
}

} // namespace dbp::publisher
