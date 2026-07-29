#pragma once

#include "dbp/package/PackageError.h"

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <vector>

namespace dbp::publisher {

enum class PublisherCommandKind {
    Help,
    Version,
    Validate,
    Publish,
};

enum class PublisherFailurePhase {
    Manifest,
    KeyGeneration,
    Publication,
};

struct PublisherArguments {
    PublisherCommandKind kind = PublisherCommandKind::Help;
    std::filesystem::path manifestPath;
    std::optional<std::filesystem::path> packageKeyFile;
    bool json = false;
};

package::PackageResult<PublisherArguments>
ParsePublisherArguments(
    const std::vector<std::wstring>& arguments);

int PublisherFailureExitCode(
    PublisherFailurePhase phase,
    package::PackageErrorCode code) noexcept;

int RunPublisherProcess(
    const std::vector<std::wstring>& arguments,
    std::ostream& standardOutput,
    std::ostream& standardError);

} // namespace dbp::publisher
