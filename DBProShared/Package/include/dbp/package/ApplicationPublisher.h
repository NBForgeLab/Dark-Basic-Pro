#pragma once

#include "dbp/package/PackageWriter.h"
#include "dbp/package/PublicationCheckpoint.h"
#include "dbp/package/RuntimeDescriptor.h"

#include <filesystem>
#include <vector>

namespace dbp::package {

struct ApplicationPublishRequest {
    std::filesystem::path hostExecutable;
    std::filesystem::path outputExecutable;
    RuntimeMode mode = RuntimeMode::Application;
    KeyId keyId{};
    std::vector<PackageSourceEntry> entries;
    PackageLimits limits;
};

struct ApplicationPublishResult {
    std::filesystem::path executablePath;
    std::filesystem::path descriptorPath;
    PackageWriteResult package;
};

class ApplicationPublisher {
public:
    ApplicationPublisher(
        const CryptoProvider& crypto,
        const ZstdCompressionCodec& compression,
        const AtomicFilePublisher& filePublisher,
        const PublicationCheckpoint& checkpoint) noexcept;

    PackageResult<ApplicationPublishResult> Publish(
        const ApplicationPublishRequest& request,
        const KeyProvider& keys) const;

private:
    const CryptoProvider& crypto_;
    const ZstdCompressionCodec& compression_;
    const AtomicFilePublisher& filePublisher_;
    const PublicationCheckpoint& checkpoint_;
};

} // namespace dbp::package
