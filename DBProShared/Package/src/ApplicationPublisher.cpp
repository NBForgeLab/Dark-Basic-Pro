#include "dbp/package/ApplicationPublisher.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace dbp::package {

namespace {

PackageResult<ApplicationPublishResult> PublicationFailure(
    std::string message) {
    return PackageResult<ApplicationPublishResult>::Failure({
        PackageErrorCode::PublicationFailed,
        std::move(message),
        std::nullopt,
    });
}

PackageResult<std::filesystem::path> AbsoluteNormalized(
    const std::filesystem::path& path) {
    std::error_code error;
    auto result = std::filesystem::absolute(path, error);
    if (error) {
        return PackageResult<std::filesystem::path>::Failure({
            PackageErrorCode::PublicationFailed,
            "Resolving an application publication path failed.",
            std::nullopt,
        });
    }
    return PackageResult<std::filesystem::path>::Success(
        result.lexically_normal());
}

} // namespace

ApplicationPublisher::ApplicationPublisher(
    const CryptoProvider& crypto,
    const ZstdCompressionCodec& compression,
    const AtomicFilePublisher& filePublisher,
    const PublicationCheckpoint& checkpoint) noexcept
    : crypto_(crypto),
      compression_(compression),
      filePublisher_(filePublisher),
      checkpoint_(checkpoint) {}

PackageResult<ApplicationPublishResult>
ApplicationPublisher::Publish(
    const ApplicationPublishRequest& request,
    const KeyProvider&) const {
    const auto host = AbsoluteNormalized(request.hostExecutable);
    const auto output = AbsoluteNormalized(request.outputExecutable);
    if (!host || !output) {
        return PublicationFailure(
            "Resolving application publication paths failed.");
    }
    if (host.value() == output.value()) {
        return PublicationFailure(
            "The host executable and output executable must differ.");
    }
    return PublicationFailure(
        "Application publication is not yet initialized.");
}

} // namespace dbp::package
