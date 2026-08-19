#include "RuntimeBundleResolver.h"

#include "PeExportInspector.h"

#include <algorithm>
#include <array>
#include <system_error>

namespace {

bool HasExport(const PeImageInfo& image, const char* name) {
    return image.exports.count(name) != 0;
}

RuntimeCapabilities DeriveCapabilities(const PeImageInfo& image) {
    RuntimeCapabilities capabilities;

    // The SDK runtime is native x64: every core export exists in exactly one
    // canonical x64-mangled form, so capability derivation checks those names
    // directly - no legacy spellings, no fallback chains.
    const bool hasPassCommandLine = HasExport(image, "?PassCmdLineHandlerPtr@@YAXPEAX@Z");
    const bool hasPassError = HasExport(image, "?PassErrorHandlerPtr@@YAXPEAX@Z");
    const bool hasPassEscape = HasExport(image, "?PassEscapePtr@@YAXPEAX@Z");
    const bool hasPassBreakout = HasExport(image, "?PassBreakOutPtr@@YAXPEAX@Z");
    const bool hasPassData = HasExport(image, "?PassDataStatementPtr@@YAXPEAD0@Z");
    const bool hasPassStructurePatterns = HasExport(image, "?PassStructurePatterns@@YAXPEAXK@Z");

    const bool hasInitDisplay = HasExport(image, "?InitDisplay@@YAKKKKKPEAUHINSTANCE__@@PEAD@Z");
    const bool hasDeleteVarItem = HasExport(image, "?DeleteSingleVariableAllocation@@YAXPEA_K@Z");

    const std::array<const char*, 10> basicLifecycleExports{
        "?PassDLLs@@YAXXZ",
        "?ConstructDLLs@@YAXXZ",
        "?CloseDisplay@@YAKXZ",
        "?DeleteVariableSpace@@YAXXZ",
        "?DeleteDataSpace@@YAXXZ",
        "?UnDimDD@@YA_K_K@Z",
        "?Sync@@YAXXZ",
        "?GetGlobPtr@@YAPEAUGlobStruct@@XZ",
        "?CreateVariableSpace@@YA_KK@Z",
        "?CreateDataSpace@@YA_KK@Z"};

    const bool hasBasicLifecycle =
        std::all_of(
            basicLifecycleExports.begin(), basicLifecycleExports.end(),
            [&image](const char* name) { return HasExport(image, name); });

    const bool hasLifecycle = hasBasicLifecycle && hasInitDisplay && hasDeleteVarItem;

    if (hasPassCommandLine &&
        hasPassError &&
        hasPassEscape &&
        hasPassBreakout &&
        hasLifecycle) {
        capabilities.insert(RuntimeCapability::CoreBootstrapV1);
    }
    if (hasPassData) {
        capabilities.insert(RuntimeCapability::CoreDataStatementsV1);
    }
    if (hasPassStructurePatterns) {
        capabilities.insert(RuntimeCapability::CoreStructurePatternsV1);
    }
    if (hasPassError) {
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

    // The SDK is native x64: DBProCore.dll must be a 64-bit PE image.
    if (inspection.value().machine != PeMachine::X64) {
        return Failure(
            RuntimeErrorCode::IncompatibleArchitecture,
            inspection.value().machine == PeMachine::X86
                ? "DBProCore.dll is a 32-bit (x86) image; the SDK requires a native x64 runtime."
                : "DBProCore.dll must be a native x64 PE image.",
            root,
            bundle.corePath);
    }

    bundle.capabilities = DeriveCapabilities(inspection.value());
    for (const auto required : requirements) {
        if (bundle.capabilities.count(required) == 0) {
            return Failure(
                RuntimeErrorCode::MissingCapability,
                "The selected runtime lacks required capability " +
                    std::to_string(static_cast<int>(required)) + ".",
                root,
                bundle.corePath,
                required);
        }
    }

    return RuntimeResult<ResolvedRuntimeBundle>::Success(std::move(bundle));
}
