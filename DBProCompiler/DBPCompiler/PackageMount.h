#pragma once

#include "VFSHooks.h"
#include "dbp/package/LegacyPckReader.h"
#include "dbp/package/PackageReader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

class PackageMount {
public:
    static dbp::package::PackageResult<std::unique_ptr<PackageMount>>
    MountV2(std::shared_ptr<dbp::package::PackageReader> reader);
    static dbp::package::PackageResult<std::unique_ptr<PackageMount>>
    MountLegacy(std::shared_ptr<dbp::package::LegacyPckReader> reader);

    ~PackageMount();
    PackageMount(const PackageMount&) = delete;
    PackageMount& operator=(const PackageMount&) = delete;

private:
    using Registration = std::pair<
        std::string,
        std::shared_ptr<const IVFSDataSource>>;

    explicit PackageMount(
        std::vector<Registration> registrations) noexcept;

    std::vector<Registration> registrations_;
};
