#include "PublicationDiagnostics.h"

namespace dbp::compiler {

std::string_view PublicationDiagnosticCode(
    const package::PackageError& error) noexcept {
    if (!error.applicationPublicationPhase) {
        return "DBP3107";
    }
    switch (*error.applicationPublicationPhase) {
    case package::ApplicationPublicationPhase::Package:
        return "DBP3105";
    case package::ApplicationPublicationPhase::Executable:
        return "DBP3106";
    case package::ApplicationPublicationPhase::Descriptor:
    case package::ApplicationPublicationPhase::Cleanup:
        return "DBP3107";
    }
    return "DBP3107";
}

CompilerStageCleanupResult CleanupCompilerStageAfterCommit(
    const std::filesystem::path& stagePath,
    const bool simulateFailure) noexcept {
    if (simulateFailure) {
        return CompilerStageCleanupResult::Deferred;
    }
    std::error_code error;
    const auto removed =
        std::filesystem::remove(stagePath, error);
    if (error) {
        return CompilerStageCleanupResult::Deferred;
    }
    return removed
        ? CompilerStageCleanupResult::Removed
        : CompilerStageCleanupResult::AlreadyAbsent;
}

} // namespace dbp::compiler
