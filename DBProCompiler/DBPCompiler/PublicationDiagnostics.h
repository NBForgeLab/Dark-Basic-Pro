#pragma once

#include "dbp/package/PackageError.h"

#include <filesystem>
#include <string_view>

namespace dbp::compiler {

std::string_view PublicationDiagnosticCode(
    const package::PackageError& error) noexcept;

enum class CompilerStageCleanupResult {
    Removed,
    AlreadyAbsent,
    Deferred,
};

CompilerStageCleanupResult CleanupCompilerStageAfterCommit(
    const std::filesystem::path& stagePath,
    bool simulateFailure = false) noexcept;

} // namespace dbp::compiler
