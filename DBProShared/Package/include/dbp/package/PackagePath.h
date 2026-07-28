#pragma once

#include "dbp/package/PackageError.h"

#include <string>
#include <string_view>
#include <vector>

namespace dbp::package {

PackageResult<std::string> NormalizePackageInputPath(
    std::string_view path);
PackageResult<std::string> ValidatePersistedPackagePath(
    std::string_view path);
PackageResult<std::vector<std::string>> ValidateAndSortPackagePaths(
    const std::vector<std::string>& paths);

} // namespace dbp::package
