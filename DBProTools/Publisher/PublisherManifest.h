#pragma once

#include "dbp/package/ApplicationPublisher.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dbp::publisher {

struct PublisherAsset {
    std::filesystem::path source;
    std::string destination;
    bool compress = true;
    std::optional<package::PackageSourceIdentity> sourceIdentity;
};

struct PublisherManifest {
    std::uint32_t schemaVersion = 1;
    std::filesystem::path hostExecutable;
    std::filesystem::path outputExecutable;
    package::RuntimeMode mode =
        package::RuntimeMode::Application;
    std::vector<PublisherAsset> assets;
};

package::PackageResult<PublisherManifest>
ReadPublisherManifest(
    const std::filesystem::path& manifestPath,
    const package::PackageLimits& limits = {});

package::ApplicationPublishRequest
BuildApplicationPublishRequest(
    const PublisherManifest& manifest,
    const package::KeyId& keyId,
    const package::PackageLimits& limits = {});

} // namespace dbp::publisher
