#include "RuntimeBundleResolver.h"

#include "PeExportInspector.h"

#include <array>
#include <system_error>

namespace {

constexpr const char* kPassCommandLine =
    "?PassCmdLineHandlerPtr@@YAXPAX@Z";
constexpr const char* kPassError =
    "?PassErrorHandlerPtr@@YAXPAX@Z";
constexpr const char* kPassEscape =
    "?PassEscapePtr@@YAXPAX@Z";
constexpr const char* kPassBreakout =
    "?PassBreakOutPtr@@YAXPAX@Z";
constexpr const char* kPassData =
    "?PassDataStatementPtr@@YAXPAD0@Z";
constexpr const char* kPassStructurePatterns =
    "?PassStructurePatterns@@YAXPAXK@Z";

bool HasExport(const PeImageInfo& image, const char* name) {
    return image.exports.count(name) != 0;
}

RuntimeCapabilities DeriveCapabilities(const PeImageInfo& image) {
    RuntimeCapabilities capabilities;
    if (HasExport(image, kPassCommandLine) &&
        HasExport(image, kPassError) &&
        HasExport(image, kPassEscape) &&
        HasExport(image, kPassBreakout)) {
        capabilities.insert(RuntimeCapability::CoreBootstrapV1);
    }
    if (HasExport(image, kPassData)) {
        capabilities.insert(RuntimeCapability::CoreDataStatementsV1);
    }
    if (HasExport(image, kPassStructurePatterns)) {
        capabilities.insert(RuntimeCapability::CoreStructurePatternsV1);
    }
    if (HasExport(image, kPassError)) {
        capabilities.insert(RuntimeCapability::CoreRuntimeErrorsV1);
    }
    return capabilities;
}

RuntimeResult<ResolvedRuntimeBundle> Failure(
    const RuntimeErrorCode code,
    const std::string& message,
    const std::filesystem::path& root,
    const std::filesystem::path& component = {},
    const std::optional<RuntimeCapability> capability = std::nullopt) {
    return RuntimeResult<ResolvedRuntimeBundle>::Failure(
        {code, message, root, component, capability});
}

} // namespace

RuntimeResult<ResolvedRuntimeBundle> RuntimeBundleResolver::Resolve(
    const RuntimeSelection& selection,
    const ProgramRuntimeRequirements& requirements) {
    std::filesystem::path selectedRoot;
    if (selection.explicitRoot) {
        selectedRoot = *selection.explicitRoot;
    } else {
        const auto versionedCandidate =
            selection.compilerDirectory.parent_path() / "runtime";
        std::error_code existsError;
        if (std::filesystem::is_directory(versionedCandidate, existsError)) {
            selectedRoot = versionedCandidate;
        } else {
            selectedRoot = selection.compilerDirectory;
        }
    }

    std::error_code canonicalError;
    const auto root = std::filesystem::weakly_canonical(
        selectedRoot, canonicalError);
    if (canonicalError || !std::filesystem::is_directory(root)) {
        return Failure(
            RuntimeErrorCode::MissingRoot,
            "The selected DBPro runtime root does not exist.",
            selectedRoot);
    }

    ResolvedRuntimeBundle bundle;
    bundle.root = root;
    bundle.pluginsDirectory = root / "plugins";
    bundle.userPluginsDirectory = root / "plugins-user";
    bundle.licensedPluginsDirectory = root / "plugins-licensed";
    bundle.effectsDirectory = root / "effects";
    bundle.corePath = bundle.pluginsDirectory / "DBProCore.dll";
    bundle.classification = std::filesystem::exists(
        root / "runtime-manifest.json")
        ? RuntimeBundleClassification::Versioned
        : RuntimeBundleClassification::LegacyUnversioned;

    if (!std::filesystem::is_regular_file(bundle.corePath)) {
        return Failure(
            RuntimeErrorCode::MissingComponent,
            "The selected runtime does not contain plugins/DBProCore.dll.",
            root,
            bundle.corePath);
    }

    const auto inspection = PeExportInspector::Inspect(bundle.corePath);
    if (!inspection) {
        return Failure(
            RuntimeErrorCode::InvalidComponent,
            inspection.error().message,
            root,
            bundle.corePath);
    }
    if (inspection.value().machine != PeMachine::X86) {
        return Failure(
            RuntimeErrorCode::IncompatibleArchitecture,
            "DBProCore.dll must be a Win32 x86 image.",
            root,
            bundle.corePath);
    }

    bundle.capabilities = DeriveCapabilities(inspection.value());
    const auto missing = MissingCapabilities(bundle.capabilities, requirements);
    if (!missing.empty()) {
        return Failure(
            RuntimeErrorCode::MissingCapability,
            "The selected DBPro runtime is missing a required capability.",
            root,
            bundle.corePath,
            *missing.begin());
    }

    return RuntimeResult<ResolvedRuntimeBundle>::Success(std::move(bundle));
}
