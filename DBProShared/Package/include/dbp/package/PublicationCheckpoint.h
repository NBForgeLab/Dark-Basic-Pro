#pragma once

#include "dbp/package/PackageError.h"

namespace dbp::package {

enum class PublicationStage {
    PackagePublished,
    ExecutablePublished,
    DescriptorPublished,
};

class PublicationCheckpoint {
public:
    virtual ~PublicationCheckpoint() = default;

    virtual PackageResult<bool> Reach(
        PublicationStage stage) const = 0;
};

class NoopPublicationCheckpoint final
    : public PublicationCheckpoint {
public:
    PackageResult<bool> Reach(
        PublicationStage stage) const override;
};

} // namespace dbp::package
