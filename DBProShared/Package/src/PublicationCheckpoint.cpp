#include "dbp/package/PublicationCheckpoint.h"

namespace dbp::package {

PackageResult<bool> NoopPublicationCheckpoint::Reach(
    const PublicationStage) const {
    return PackageResult<bool>::Success(true);
}

} // namespace dbp::package
